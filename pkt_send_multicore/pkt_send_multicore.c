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
#include <getopt.h>
#include <unistd.h> // For sleep()
#include <math.h> //For pow()

#define APP_ETHER_TYPE  0x2222
#define APP_MAGIC       0x3333

#define MAX_LCORES 72
#define RX_QUEUE_PER_LCORE   1
#define TX_QUEUE_PER_LCORE   1
#define MAX_BURST_SIZE       32 //length of queue
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 4095
#define MBUF_SIZE   (10000+sizeof(struct rte_mbuf)+RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 32
#define BURST_SIZE 32
#define PKT_LEN 64
#define PAY_LOAD_LEN (PKT_LEN-28) //udp

#define APP_LOG(...) RTE_LOG(INFO, USER1, __VA_ARGS__)
// #define PRN_COLOR(str) ("\033[0;33m" str "\033[0m")	// Yellow accent

#define FLOW_NUM 100

#define ZIPF_A 0.75
#define ZIPF_C 1.0

struct lcore_configuration {
    uint32_t vid; // virtual core id
    uint32_t port; // one port
    uint32_t n_tx_queue;  // number of TX queues
    uint32_t tx_queue_list[TX_QUEUE_PER_LCORE]; // list of TX queues
    uint32_t n_rx_queue;  // number of RX queues
    uint32_t rx_queue_list[RX_QUEUE_PER_LCORE]; // list of RX queues
} __rte_cache_aligned;

struct payload
{
    char data[PAY_LOAD_LEN];/* data */
};

struct rte_ether_addr src_mac;
struct rte_ether_addr dst_mac;

struct rte_mempool *pktmbuf_pool[MAX_LCORES];
static unsigned enabled_port = 0;
static uint64_t min_txintv = 15/*us*/;	// min cycles between 2 returned packets
static volatile bool force_quit = false;
static volatile uint16_t nb_pending = 0;

struct lcore_configuration lcore_conf[MAX_LCORES];

uint64_t tx_pkt_num[MAX_LCORES];
uint64_t rx_pkt_num[MAX_LCORES];
double tx_pps[MAX_LCORES];
double rx_pps[MAX_LCORES];
double tx_bps[MAX_LCORES];
double rx_bps[MAX_LCORES];

// struct rte_ether_addr dst_mac;
// struct rte_ether_addr src_mac;

void zipf_generate(double *zipf_cumuP){
    double sum = 0.0;
    int i;

    for (i = 0; i < FLOW_NUM; i++){         
        sum += ZIPF_C/pow((double)(i+2), ZIPF_A);
    }
    for (i = 0; i < FLOW_NUM; i++){ 
        double zipf_temp = ZIPF_C/pow((double)(i+2), ZIPF_A);
        if (i == 0)
            zipf_cumuP[i] = zipf_temp/sum;
        else
            zipf_cumuP[i] = zipf_cumuP[i-1] + zipf_temp/sum;
    }
}

int zipf_pick(const double *zipf_cumuP){
    unsigned int seed = rte_rdtsc();
    int index = 0;
    double data = (double)rand_r(&seed)/RAND_MAX;  //生成一个0~1的数
    while (data > zipf_cumuP[index])   //找索引,直到找到一个比他小的值,那么对应的index就是随机数了
        index++;
    return index;
}

uint32_t reversebytes_uint32t(uint32_t value){
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 | 
        (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24; 
}

static void
fill_ethernet_header(struct rte_ether_hdr *eth_hdr) {
	struct rte_ether_addr s_addr = src_mac; 
	struct rte_ether_addr d_addr = dst_mac;
	eth_hdr->src_addr =s_addr;
	eth_hdr->dst_addr =d_addr;
	eth_hdr->ether_type = rte_cpu_to_be_16(0x0800);
}

static void
fill_ipv4_header(struct rte_ipv4_hdr *ipv4_hdr, const double *zipf_cumuP) {
	ipv4_hdr->version_ihl = (4 << 4) + 5; // ipv4, length 5 (*4)
	ipv4_hdr->type_of_service = 0; // No Diffserv
	ipv4_hdr->total_length = rte_cpu_to_be_16(PKT_LEN); // tcp 20
	ipv4_hdr->packet_id = rte_cpu_to_be_16(5462); // set random
	ipv4_hdr->fragment_offset = rte_cpu_to_be_16(0);
	ipv4_hdr->time_to_live = 64;
	ipv4_hdr->next_proto_id = 17; // udp
	ipv4_hdr->hdr_checksum = rte_cpu_to_be_16(0x0);

    unsigned int seed = rte_rdtsc();
    uint32_t src_ip = rand_r(&seed)%FLOW_NUM;
    // uint32_t src_ip = zipf_pick(zipf_cumuP);
    // uint32_t src_ip = inet_addr("192.168.200.2");// cx2
    uint32_t dst_ip = inet_addr("192.168.200.1");// bf4
	ipv4_hdr->src_addr = rte_cpu_to_be_32(reversebytes_uint32t(src_ip)); 
	ipv4_hdr->dst_addr = rte_cpu_to_be_32(reversebytes_uint32t(dst_ip)); 

	ipv4_hdr->hdr_checksum = rte_cpu_to_be_16(rte_ipv4_cksum(ipv4_hdr));
}

static void
fill_udp_header(struct rte_udp_hdr *udp_hdr, struct rte_ipv4_hdr *ipv4_hdr, const double *zipf_cumuP) {
	udp_hdr->src_port = rte_cpu_to_be_16(0x162E);
    int dst_port = zipf_pick(zipf_cumuP);
	udp_hdr->dst_port = rte_cpu_to_be_16(dst_port);
    // udp_hdr->dst_port = rte_cpu_to_be_16(0x1503);
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

static struct rte_mbuf *make_testpkt(uint32_t queue_id, double *zipf_cumuP)
{
    
    struct rte_mbuf *mp = rte_pktmbuf_alloc(pktmbuf_pool[queue_id]);

    uint16_t pkt_len = PKT_LEN+sizeof(struct rte_ether_hdr);
    char *buf = rte_pktmbuf_append(mp, pkt_len);
    if (unlikely(buf == NULL)) {
        APP_LOG("Error: failed to allocate packet buffer.\n");
        rte_pktmbuf_free(mp);
        return NULL;
    }

    mp->data_len = pkt_len;
    mp->pkt_len = pkt_len;
    uint16_t curr_ofs = 0;

    struct rte_ether_hdr *ether_h = rte_pktmbuf_mtod_offset(mp, struct rte_ether_hdr *, curr_ofs);
	fill_ethernet_header(ether_h);
    curr_ofs += sizeof(struct rte_ether_hdr);

    struct rte_ipv4_hdr *ipv4_h = rte_pktmbuf_mtod_offset(mp, struct rte_ipv4_hdr *, curr_ofs);
	fill_ipv4_header(ipv4_h, zipf_cumuP);
    curr_ofs += sizeof(struct rte_ipv4_hdr);

    // struct rte_tcp_hdr *tcp_h = rte_pktmbuf_mtod_offset(mp, struct rte_tcp_hdr *, curr_ofs);
	// fill_tcp_header(tcp_h, ipv4_h);
    // curr_ofs += sizeof(struct rte_tcp_hdr);

    struct rte_udp_hdr *udp_h = rte_pktmbuf_mtod_offset(mp, struct rte_udp_hdr *, curr_ofs);
	fill_udp_header(udp_h, ipv4_h,zipf_cumuP);
    curr_ofs += sizeof(struct rte_udp_hdr);
    
    // struct payload * payload_data= rte_pktmbuf_mtod_offset(mp, struct payload *, curr_ofs);
    // payload_data->data[0] = 1;

    return mp;
}

/*
 * The lcore main. This is the main thread that does the work, reading from
 * an input port and writing to an output port.
 */
static void lcore_main(uint32_t lcore_id)
{
    // Check that the port is on the same NUMA node.
    if (rte_eth_dev_socket_id(enabled_port) > 0 &&
            rte_eth_dev_socket_id(enabled_port) != (int)rte_socket_id())
        printf("WARNING, port %u is on remote NUMA node.\n", enabled_port);
    printf("Core %u forwarding packets. [Ctrl+C to quit]\n", rte_lcore_id());
    fflush(stdout);

    /* Run until the application is quit or killed. */
    uint16_t tx_count = 0;
    struct lcore_configuration *lconf = &lcore_conf[lcore_id];
    struct rte_mbuf *bufs[BURST_SIZE], *bufs_tx[BURST_SIZE];
    uint64_t last_tx_tsc = 0;
    uint64_t perf_runtime_tsc = rte_rdtsc();	// Save starting time first
    uint64_t loop_count = 0, total_tx = 0, total_rx = 0;
    
    double pf[FLOW_NUM];
    zipf_generate(pf);
    
    uint64_t start = rte_rdtsc();
    uint64_t length = 0;
    while (!force_quit/* && loop_count < 1000*/) {
        int i;
        for (i = 0; i < lconf->n_rx_queue; i++){
            int j;
            for(j = 0; j < BURST_SIZE; j++){
                bufs_tx[j] = make_testpkt(lconf->rx_queue_list[i],pf);
            }
            // Transmit packet
            uint16_t nb_tx = rte_eth_tx_burst(lconf->port, lconf->rx_queue_list[i], bufs_tx, BURST_SIZE);
            total_tx += nb_tx;
            length += (bufs_tx[0]->data_len*nb_tx);
            if (nb_tx < BURST_SIZE){
                rte_pktmbuf_free_bulk(bufs_tx, BURST_SIZE - nb_tx);
            }
        }
        loop_count++;
    }
    // uint64_t time_interval = rte_rdtsc() - start;
    double time_interval = (double)(rte_rdtsc() - start)/rte_get_timer_hz();
    APP_LOG("run time: %lf.\n", time_interval);
    APP_LOG("Sent %ld pkts, received %ld pkts, throughput: %lf pps, %lf bps.\n", total_tx, total_rx, (double)total_tx/time_interval, (double)length*8/time_interval);
    APP_LOG("times of loop is %ld, should send packets %ld.\n", loop_count, loop_count);
    tx_pkt_num[lcore_id] = total_tx;
    rx_pkt_num[lcore_id] = total_rx;
    tx_pps[lcore_id] = (double)total_tx/time_interval;
    tx_bps[lcore_id] = (double)length*8/time_interval;
}

static int
launch_one_lcore(__attribute__((unused)) void *arg){
    uint32_t lcore_id = rte_lcore_id();
	lcore_main(lcore_id);
	return 0;
}

int mac_read(char * mac_p, struct rte_ether_addr *mac_addr){
    char mac_char[20];
    char *pchStr;	
    int  nTotal = 0;

    strcpy(mac_char, mac_p);
    pchStr = strtok(mac_char, ":");
	
    while (NULL != pchStr)
	{
    	mac_addr->addr_bytes[nTotal++] = strtol(pchStr,NULL,16);

    	pchStr = strtok(NULL, ":");
	}
}

/* Parse the argument given in the command line of the application */
static int
parse_app_opts(int argc, char **argv)
{
	int opt, ret;
	char **argvopt;
	int option_index = 0;
	char *prgname = argv[0];
	static struct option long_options[] = {
		{"srcmac", 1, NULL, '1'},
        {"dstmac", 1, NULL, '2'}

	};

	argvopt = argv;

	while ((opt = getopt_long(argc, argvopt, "1:2:",
				  long_options, &option_index)) != EOF) {

		switch (opt) {
		/* portmask */
		case '1':
            mac_read(optarg, &src_mac);
			break;
		case '2':
            mac_read(optarg, &dst_mac);
			break;
		/* long options */
		default:
			rte_exit(EXIT_FAILURE, "Error: Wrong use of application arguments\n");
			return -1;
		}
	}

	if (optind >= 0)
		argv[optind-1] = prgname;

	ret = optind-1;
	optind = 1; /* reset getopt lib */

	return ret;
}




/* Main functional part of port initialization. 8< */
static inline int
port_init(uint16_t port)
{
	struct rte_eth_conf port_conf;
	uint16_t rx_rings = 0, tx_rings = 0;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;
	int retval;
	uint16_t q;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_txconf txconf;
    uint32_t n_lcores = 0;

    int i, j;


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
    
    printf("create enabled cores\n\tcores: ");
    n_lcores = 0;
    for(i = 0; i < MAX_LCORES; i++) {
        if(rte_lcore_is_enabled(i)) {
            n_lcores++;
            printf("%u ",i);
        }
    }
    printf("\n");

    // assign each lcore some RX & TX queues and a port
    uint32_t rx_queues_per_lcore = RX_QUEUE_PER_LCORE;
    uint32_t tx_queues_per_lcore = TX_QUEUE_PER_LCORE;
    rx_rings = n_lcores * RX_QUEUE_PER_LCORE;
    tx_rings = n_lcores * TX_QUEUE_PER_LCORE;

    uint32_t rx_queue_id = 0;
    uint32_t tx_queue_id = 0;
    uint32_t vid = 0;
    for (i = 0; i < MAX_LCORES; i++) {
        if(rte_lcore_is_enabled(i)) {
            lcore_conf[i].vid = vid++;
            lcore_conf[i].n_rx_queue = rx_queues_per_lcore;
            for (j = 0; j < lcore_conf[i].n_rx_queue; j++) {
                lcore_conf[i].rx_queue_list[j] = rx_queue_id++;
            }
            lcore_conf[i].n_tx_queue = tx_queues_per_lcore;
            for (j = 0; j < lcore_conf[i].n_tx_queue; j++) {
                lcore_conf[i].tx_queue_list[j] = tx_queue_id++;
            }
            lcore_conf[i].port = enabled_port;

        }
    }

	/* Configuration the Ethernet device. */
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0){
        rte_exit(EXIT_FAILURE,
                "Ethernet device configuration error: err=%d, port=%u\n", retval, port);
    }

	retval = rte_eth_dev_adjust_nb_rx_tx_desc(port, &nb_rxd, &nb_txd);
	if (retval != 0)
		return retval;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < rx_rings; q++) {
        // create mbuf pool
        printf("create mbuf_pool_%d\n",q);
        char name[50];
        sprintf(name,"mbuf_pool_%d",q);
        pktmbuf_pool[q] = rte_mempool_create(
            name,
            NUM_MBUFS,
            MBUF_SIZE,
            MBUF_CACHE_SIZE,
            sizeof(struct rte_pktmbuf_pool_private),
            rte_pktmbuf_pool_init, NULL,
            rte_pktmbuf_init, NULL,
            rte_socket_id(),
            0);
        if (pktmbuf_pool[q] == NULL) {
            rte_exit(EXIT_FAILURE, "cannot init mbuf_pool_%d\n", q);
        }

        retval = rte_eth_rx_queue_setup(port, q, nb_rxd,
                rte_eth_dev_socket_id(port), NULL, pktmbuf_pool[q]);
        if (retval < 0) {
            rte_exit(EXIT_FAILURE,
                "rte_eth_rx_queue_setup: err=%d, port=%u\n", retval, port);
        }
	}

	txconf = dev_info.default_txconf;
	txconf.offloads = port_conf.txmode.offloads;
	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, nb_txd,
				rte_eth_dev_socket_id(port), &txconf);
		if (retval < 0){
			rte_exit(EXIT_FAILURE,
                "rte_eth_tx_queue_setup: err=%d, port=%u\n", retval, port);
        }
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
    unsigned lcore_id;

    /* Initialize the Environment Abstraction Layer (EAL). */
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
    argc -= ret; argv += ret;

    /* Parse app-specific arguments. */
	/* parse application arguments (after the EAL ones) */
	ret = parse_app_opts(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error: Wrong use of application arguments\n");

    /* Setup signal handler. */
    force_quit = false;
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Check that there is an even number of ports to send/receive on. */
    nb_ports = rte_eth_dev_count_avail();
    if (enabled_port >= nb_ports)
        rte_exit(EXIT_FAILURE, "Error: Specified port out-of-range\n");

    /* Initialize the ports. */
    if (port_init(enabled_port) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %u\n", enabled_port);

    rte_eal_mp_remote_launch((lcore_function_t *)launch_one_lcore, NULL, CALL_MAIN);
    RTE_LCORE_FOREACH_WORKER(lcore_id){
        if (rte_eal_wait_lcore(lcore_id) < 0) {
            ret = -1;
            break;
        } 
    }

    int i;
    uint64_t total_tx_pkt_num = 0, total_rx_pkt_num = 0;
    double total_tx_pps = 0.0, total_tx_bps = 0.0;
    for(i = 0; i < MAX_LCORES; i++){
        total_tx_pkt_num += tx_pkt_num[i];
        total_rx_pkt_num +=rx_pkt_num[i];
        total_tx_pps += tx_pps[i];
        total_tx_bps += tx_bps[i];
    }
    APP_LOG("Sent %ld pkts, received %ld pkts, throughput: %lf pps, %lf bps.\n", total_tx_pkt_num, total_rx_pkt_num, total_tx_pps, total_tx_bps);
 
    // /* Calculate minimum TSC diff for returning signed packets */
    // min_txintv *= rte_get_timer_hz() / US_PER_S;
    // printf("TSC freq: %lu Hz\n", rte_get_timer_hz());

    /* Cleaning up. */
    fflush(stdout);
APP_RTE_CLEANUP:
    printf("Closing port %u... ", enabled_port);
    rte_eth_dev_stop(enabled_port);
    rte_eth_dev_close(enabled_port);
    printf("Done.\n");

    return 0;
}
