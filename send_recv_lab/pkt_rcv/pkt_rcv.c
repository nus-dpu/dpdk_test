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
#define BURST_SIZE 64

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

static struct rte_mbuf *make_testpkt(uint16_t opcode, uint16_t nblks, uint64_t lba)
{
    struct rte_mbuf *mp = rte_pktmbuf_alloc(mbuf_pool);
    uint16_t pkt_len = 128; // 2 beats of 64B for ether and command
    if ((opcode & 0x3) == 1)  // Write command, reserve for data buffer
        pkt_len += nblks * 512;
    char *buf = rte_pktmbuf_append(mp, pkt_len);
    if (unlikely(buf == NULL)) {
        APP_LOG("Error: failed to allocate packet buffer.\n");
        rte_pktmbuf_free(mp);
        return NULL;
    }
    mp->data_len = pkt_len;
    uint16_t curr_ofs = 0;
    // Write Ethernet header
    GET_RTE_HDR(rte_ether_hdr, ether, mp, curr_ofs);
    ether->dst_addr = target_mac;
    ether->src_addr = mac_addr;
    ether->ether_type = rte_cpu_to_be_16(APP_ETHER_TYPE);
    curr_ofs += RTE_ETHER_HDR_LEN;
    // Write NVMe tester header
    GET_RTE_HDR(nvmetest_pkthdr, nvmehdr, mp, curr_ofs);
    nvmehdr->magic = rte_cpu_to_be_16(APP_MAGIC);
    nvmehdr->opcode = rte_cpu_to_le_32(opcode);
    nvmehdr->nblks = rte_cpu_to_le_32(nblks);
    nvmehdr->lba = rte_cpu_to_le_64(lba);
    curr_ofs += sizeof(struct nvmetest_pkthdr);
    // Append data if we are at a write command
    if ((opcode & 0x3) == 1) {
        unsigned i = 0;
        for (i = 0; i < nblks * 8; i++) {  // 8 beats per 512B blk
            memset(buf + curr_ofs, (uint8_t)i, 64);
            curr_ofs += 64;
        }
    }
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
    struct rte_mbuf *bufs[BURST_SIZE];
    uint64_t last_tx_tsc = 0;
    uint64_t loop_count = 0, total_rx = 0, total_tx = 0;

    uint64_t start = rte_rdtsc();
    
    while (!force_quit/* && loop_count < 1000*/) {
        const uint16_t nb_rx = 
		rte_eth_rx_burst(enabled_port, 0, bufs, BURST_SIZE);
        total_rx += nb_rx;

        rte_pktmbuf_free_bulk(bufs, nb_rx);
        loop_count++;
    }
    uint64_t time_interval = rte_rdtsc() - start;
    double run_time = (double)time_interval/rte_get_timer_hz();
    APP_LOG("run time: %lf.\n", run_time);
    APP_LOG("Sent %ld pkts, received %ld pkts.\n", total_tx, total_rx);
    APP_LOG("times of loop is %ld, should send packets %ld.\n", loop_count, loop_count*32);
    APP_LOG("throughput %f pps.\n", total_rx/run_time);

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
