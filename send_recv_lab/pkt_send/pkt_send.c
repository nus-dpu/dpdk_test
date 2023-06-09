#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <inttypes.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_mbuf.h>
#include <rte_prefetch.h>
#include <rte_ether.h>
#include <rte_ip.h>
#include <unistd.h> // For sleep()

#define APP_ETHER_TYPE  0x2222
#define APP_MAGIC       0x3333
static const struct rte_ether_addr target_mac = {
    .addr_bytes = {0x00, 0x0A, 0x35, 0x01, 0x02, 0x03}
};

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define PKT_LEN 1500
#define PAY_LOAD_LEN PKT_LEN - sizeof(struct rte_ipv4_hdr)\
                             - sizeof(struct rte_udp_hdr)

#define GET_RTE_HDR(t, h, m, o) \
    struct t *h = rte_pktmbuf_mtod_offset(m, struct t *, o)
#define APP_LOG(...) RTE_LOG(INFO, USER1, __VA_ARGS__)
#define PRN_COLOR(str) ("\033[0;33m" str "\033[0m")	// Yellow accent

struct rte_mempool *mbuf_pool;
static unsigned enabled_port = 0;
static struct rte_ether_addr mac_addr;
static uint64_t min_txintv = 15/*us*/;	// min cycles between 2 returned packets
static volatile bool force_quit = false;
static volatile uint16_t nb_pending = 0;

struct nvmetest_pkthdr {
    rte_be16_t magic;
    uint8_t padding_a[48];
    rte_le32_t opcode;
    rte_le32_t nblks;
    rte_le64_t lba;
    uint8_t padding_b[48];
} __rte_packed;

struct message {
	char data[PAY_LOAD_LEN];
};

static void
fill_ethernet_header(struct rte_ether_hdr *eth_hdr) {
    struct rte_ether_addr s_addr = {{0xB8,0x83,0x03,0x82,0xA2,0x10}};//cx4
	struct rte_ether_addr d_addr = {{0x0C,0x42,0xA1,0xD8,0x10,0x84}};//bf2
	eth_hdr->src_addr =s_addr;
	eth_hdr->dst_addr =d_addr;
	eth_hdr->ether_type = rte_cpu_to_be_16(0x0800);
}

uint32_t reversebytes_uint32t(uint32_t value){
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 | 
        (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24; 
}

static void
fill_ipv4_header(struct rte_ipv4_hdr *ipv4_hdr) {
	ipv4_hdr->version_ihl = (4 << 4) + 5; // ipv4, length 5 (*4)
	ipv4_hdr->type_of_service = 0; // No Diffserv
	ipv4_hdr->total_length = rte_cpu_to_be_16(PKT_LEN); 
	ipv4_hdr->packet_id = rte_cpu_to_be_16(5462); // set random
	ipv4_hdr->fragment_offset = rte_cpu_to_be_16(0x0);
	ipv4_hdr->time_to_live = 0x40;
	ipv4_hdr->next_proto_id = 0x11; // tcp:6, udp:17
	ipv4_hdr->hdr_checksum = rte_cpu_to_be_16(0x0);

    uint32_t src_ip = inet_addr("192.168.200.2");// cx4
    uint32_t dst_ip = inet_addr("192.168.200.1");// bf2
	ipv4_hdr->src_addr = rte_cpu_to_be_32(reversebytes_uint32t(src_ip)); 
	ipv4_hdr->dst_addr = rte_cpu_to_be_32(reversebytes_uint32t(dst_ip)); 

	ipv4_hdr->hdr_checksum = rte_cpu_to_be_16(rte_ipv4_cksum(ipv4_hdr));
}

static void
fill_udp_header(struct rte_udp_hdr *udp_hdr, struct rte_ipv4_hdr *ipv4_hdr) {
	udp_hdr->src_port = rte_cpu_to_be_16(0x162E);
	udp_hdr->dst_port = rte_cpu_to_be_16(0x04d2);
	udp_hdr->dgram_len = rte_cpu_to_be_16(PKT_LEN - sizeof(struct rte_ipv4_hdr));
    udp_hdr->dgram_cksum = rte_cpu_to_be_16(0x0);
	
	udp_hdr->dgram_cksum = rte_cpu_to_be_16(rte_ipv4_udptcp_cksum(ipv4_hdr, udp_hdr));
}

static void
fill_tcp_header(struct rte_tcp_hdr *tcp_hdr, struct rte_ipv4_hdr *ipv4_hdr) {
	tcp_hdr->src_port = rte_cpu_to_be_16(0x162E);
	tcp_hdr->dst_port = rte_cpu_to_be_16(0x04d2);
	tcp_hdr->sent_seq = rte_cpu_to_be_32(0);
	tcp_hdr->recv_ack = rte_cpu_to_be_32(0);
	tcp_hdr->data_off = 0;
	tcp_hdr->tcp_flags = 0;
	tcp_hdr->rx_win = rte_cpu_to_be_16(16);
	tcp_hdr->cksum = rte_cpu_to_be_16(0x0);
	tcp_hdr->tcp_urp = rte_cpu_to_be_16(0);

	tcp_hdr->cksum = rte_cpu_to_be_16(rte_ipv4_udptcp_cksum(ipv4_hdr, tcp_hdr));
}

static struct rte_mbuf *make_testpkt(void)
{
    
    struct rte_mbuf *mp = rte_pktmbuf_alloc(mbuf_pool);
    struct rte_ether_hdr *ether_h;
	struct rte_ipv4_hdr *ipv4_h;
	struct rte_tcp_hdr *tcp_h;
    struct rte_udp_hdr *udp_h;
    struct message *pay_load;
    struct message data = {{"1"}};

	ether_h = (struct rte_ether_hdr *) rte_pktmbuf_append(mp, sizeof(struct rte_ether_hdr));
	fill_ethernet_header(ether_h);

	ipv4_h = (struct rte_ipv4_hdr *) rte_pktmbuf_append(mp, sizeof(struct rte_ipv4_hdr));
	fill_ipv4_header(ipv4_h);

	// tcp_h = (struct rte_tcp_hdr *) rte_pktmbuf_append(mp, sizeof(struct rte_tcp_hdr));
	// fill_tcp_header(tcp_h, ipv4_h);

    udp_h = (struct rte_udp_hdr *) rte_pktmbuf_append(mp, sizeof(struct rte_udp_hdr));
	fill_udp_header(udp_h, ipv4_h);

    // uint16_t *udp_len = rte_pktmbuf_mtod_offset(mp, uint16_t *, 0x26);
    // printf("UDP Len: %x\n", *udp_len);
    
    pay_load = (struct message *) rte_pktmbuf_append(mp, PAY_LOAD_LEN);
    *pay_load = data;


    // uint16_t pkt_len = 128; // 2 beats of 64B for ether and command
    // if ((opcode & 0x3) == 1)  // Write command, reserve for data buffer
    //     pkt_len += nblks * 512;
    // char *buf = rte_pktmbuf_append(mp, pkt_len);
    // if (unlikely(buf == NULL)) {
    //     APP_LOG("Error: failed to allocate packet buffer.\n");
    //     rte_pktmbuf_free(mp);
    //     return NULL;
    // }
    // mp->data_len = pkt_len;
    // uint16_t curr_ofs = 0;
    // // Write Ethernet header
    // GET_RTE_HDR(rte_ether_hdr, ether, mp, curr_ofs);
    // ether->dst_addr = target_mac;
    // ether->src_addr = mac_addr;
    // ether->ether_type = rte_cpu_to_be_16(APP_ETHER_TYPE);
    // curr_ofs += RTE_ETHER_HDR_LEN;
    // // Write NVMe tester header
    // GET_RTE_HDR(nvmetest_pkthdr, nvmehdr, mp, curr_ofs);
    // nvmehdr->magic = rte_cpu_to_be_16(APP_MAGIC);
    // nvmehdr->opcode = rte_cpu_to_le_32(opcode);
    // nvmehdr->nblks = rte_cpu_to_le_32(nblks);
    // nvmehdr->lba = rte_cpu_to_le_64(lba);
    // curr_ofs += sizeof(struct nvmetest_pkthdr);
    // // Append data if we are at a write command
    // if ((opcode & 0x3) == 1) {
    //     unsigned i = 0;
    //     for (i = 0; i < nblks * 8; i++) {  // 8 beats per 512B blk
    //         memset(buf + curr_ofs, (uint8_t)i, 64);
    //         curr_ofs += 64;
    //     }
    // }
    return mp;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static void lcore_main(void)
{
    // Check that the port is on the same NUMA node.
    if (rte_eth_dev_socket_id(enabled_port) > 0 &&
            rte_eth_dev_socket_id(enabled_port) != (int)rte_socket_id())
        printf("WARNING, port %u is on remote NUMA node.\n", enabled_port);
    printf("Core %u forwarding packets. [Ctrl+C to quit]\n", rte_lcore_id());
    fflush(stdout);

    /* Run until the application is quit or killed. */
    uint16_t tx_count = 0;
    struct rte_mbuf *bufs[BURST_SIZE], *bufs_tx[BURST_SIZE];
    uint64_t last_tx_tsc = 0;
    uint64_t perf_runtime_tsc = rte_rdtsc();	// Save starting time first
    uint64_t loop_count = 0, total_tx = 0, total_rx = 0;
    
    uint64_t start = rte_rdtsc();
    uint64_t length = 0;
    while (!force_quit/* && loop_count < 1000*/) {
        // Transmit packet
        bufs_tx[0] = make_testpkt();
        uint16_t tx_count = 1;
        uint16_t nb_tx = rte_eth_tx_burst(enabled_port, 0, bufs_tx, tx_count);
        total_tx += nb_tx;
        length += bufs_tx[0]->data_len;
        if (nb_tx < tx_count){
            rte_pktmbuf_free_bulk(bufs_tx + nb_tx, tx_count - nb_tx);
        }
        loop_count++;
        // if (total_tx < 500 || 1) {
        //     if (total_tx % 3)
        //         bufs_tx[0] = make_testpkt(0x102, 2, 0x3);
        //     else
        //         bufs_tx[0] = make_testpkt(0x101, 2, 0x3);
        //     uint16_t tx_count = 1;
        //     uint16_t nb_tx = rte_eth_tx_burst(enabled_port, 0, bufs_tx, tx_count);
        //     total_tx += nb_tx;
        //     rte_pktmbuf_free_bulk(bufs, tx_count);
        // }
    }
    // uint64_t time_interval = rte_rdtsc() - start;
    double time_interval = (double)(rte_rdtsc() - start)/rte_get_timer_hz();
    APP_LOG("run time: %lf.\n", time_interval);
    APP_LOG("Sent %ld pkts, received %ld pkts, throughput: %lf pps, %lf bps.\n", total_tx, total_rx, (double)total_tx/time_interval, (double)length/time_interval);
    APP_LOG("times of loop is %ld, should send packets %ld.\n", loop_count, loop_count*32);

}

/* Main functional part of port initialization. 8< */
static inline int
port_init(uint16_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf;
	const uint16_t rx_rings = 1, tx_rings = 1;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;

	if (!rte_eth_dev_is_valid_port(port))
		return -1;

	memset(&port_conf, 0, sizeof(struct rte_eth_conf));

	retval = rte_eth_dev_info_get(port, &dev_info);
	if (retval != 0) {
		printf("Error during getting device (port %u) info: %s\n",
				port, strerror(-retval));
		return retval;
	}

	if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
		port_conf.txmode.offloads |=
			RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	/* Configure the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
		return retval;

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
				rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
			return retval;
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0)
			return retval;
	}

	/* Starting Ethernet port. 8< */
	retval = rte_eth_dev_start(port);
	/* >8 End of starting of ethernet port. */
	if (retval < 0)
		return retval;

	/* Display the port MAC address. */
	struct rte_ether_addr addr;
	retval = rte_eth_macaddr_get(port, &addr);
	if (retval != 0)
		return retval;

	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			port, RTE_ETHER_ADDR_BYTES(&addr));

	/* Enable RX in promiscuous mode for the Ethernet device. */
	retval = rte_eth_promiscuous_enable(port);
	/* End of setting RX port in promiscuous mode. */
	if (retval != 0)
		return retval;

	return 0;
}
/* >8 End of main functional part of port initialization. */


static void signal_handler(int signum)
{
    if (signum == SIGINT || signum == SIGTERM) {
        printf("\n\nSignal %d received, preparing to exit...\n", signum);
        force_quit = true;
    }
}

int main(int argc, char *argv[])
{
    unsigned nb_ports;

    /* Initialize the Environment Abstraction Layer (EAL). */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    argc -= ret; argv += ret;

    /* Parse app-specific arguments. */
    static const char user_opts[] = "p:";	// port_num
    int opt;
    while ((opt = getopt(argc, argv, user_opts)) != EOF) {
        switch (opt) {
        case 'p':
            if (optarg[0] == '\0') rte_exit(EXIT_FAILURE, "Invalid port\n");
            enabled_port = strtoul(optarg, NULL, 10);
            break;
        default: rte_exit(EXIT_FAILURE, "Invalid arguments\n");
        }
    }

    /* Setup signal handler. */
    force_quit = false;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Check that there is an even number of ports to send/receive on. */
    nb_ports = rte_eth_dev_count_avail();
    if (enabled_port >= nb_ports)
        rte_exit(EXIT_FAILURE, "Error: Specified port out-of-range\n");

    /* Creates a new mempool in memory to hold the mbufs. */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
        MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

    /* Initialize the ports. */
    if (port_init(enabled_port, mbuf_pool) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %u\n", enabled_port);

    if (rte_lcore_count() > 1)
        printf("\nWARNING: Too many lcores enabled. Only 1 used.\n");

    /* Calculate minimum TSC diff for returning signed packets */
    min_txintv *= rte_get_timer_hz() / US_PER_S;
    printf("TSC freq: %lu Hz\n", rte_get_timer_hz());

    /* Call lcore_main on the main core only. */
    lcore_main();

    /* Cleaning up. */
    fflush(stdout);
APP_RTE_CLEANUP:
    printf("Closing port %u... ", enabled_port);
    rte_eth_dev_stop(enabled_port);
    rte_eth_dev_close(enabled_port);
    printf("Done.\n");

    return 0;
}
