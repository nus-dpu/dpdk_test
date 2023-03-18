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
#include <sys/time.h> //For gettimeofday()

#include "para.h"

#define APP_ETHER_TYPE  0x2222
#define APP_MAGIC       0x3333

#define MAX_LCORES 72
#define RX_QUEUE_PER_LCORE   1
#define TX_QUEUE_PER_LCORE   1

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define NUM_MBUFS 4095
#define MBUF_SIZE   (2048+sizeof(struct rte_mbuf)+RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 32
#define BURST_SIZE 64

#define GET_RTE_HDR(t, h, m, o) \
    struct t *h = rte_pktmbuf_mtod_offset(m, struct t *, o)
#define APP_LOG(...) RTE_LOG(INFO, USER1, __VA_ARGS__)
#define PRN_COLOR(str) ("\033[0;33m" str "\033[0m")	// Yellow accent

struct rte_eth_conf port_conf = {
	.rxmode = {
		.mq_mode = RTE_ETH_MQ_RX_RSS,
	},
	.rx_adv_conf = {
		.rss_conf = {
			.rss_key = NULL,
			.rss_hf = RTE_ETH_RSS_IP | RTE_ETH_RSS_TCP | RTE_ETH_RSS_UDP,
		},
	},
};

struct lcore_configuration {
    uint32_t vid; // virtual core id
    uint32_t port; // one port
    uint32_t n_tx_queue;  // number of TX queues
    uint32_t tx_queue_list[TX_QUEUE_PER_LCORE]; // list of TX queues
    uint32_t n_rx_queue;  // number of RX queues
    uint32_t rx_queue_list[RX_QUEUE_PER_LCORE]; // list of RX queues
} __rte_cache_aligned;

static unsigned enabled_port = 0;
static struct rte_ether_addr mac_addr;
static uint64_t min_txintv = 15/*us*/;	// min cycles between 2 returned packets
static volatile bool force_quit = false;
static volatile uint16_t nb_pending = 0;

struct lcore_configuration lcore_conf[MAX_LCORES];
struct rte_mempool *pktmbuf_pool[MAX_LCORES];

uint64_t tx_pkt_num[MAX_LCORES];
uint64_t rx_pkt_num[MAX_LCORES];
uint64_t tx_pps[MAX_LCORES];
uint64_t rx_pps[MAX_LCORES];
double tx_bps[MAX_LCORES];
double rx_bps[MAX_LCORES];
double rx_pps_timeline[MAX_LCORES][MAX_RECORD_COUNT];
double rx_bps_timeline[MAX_LCORES][MAX_RECORD_COUNT];
// double (*rx_pps_timeline)[128] = (double(*)[128])malloc(sizeof(double) * 72 * 128);
// double (*rx_bps_timeline)[128] = (double(*)[128])malloc(sizeof(double) * 72 * 128);
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
    struct rte_mbuf *bufs[BURST_SIZE];
    uint64_t last_tx_tsc = 0;
    uint64_t loop_count = 0, total_rx = 0, total_tx = 0;

    uint64_t start = rte_rdtsc();
    uint64_t length = 0;


    uint64_t time_last_print = start;
    uint64_t last_total_rx = total_rx;
    uint64_t last_length = length;
    uint64_t drx;
    uint64_t dlength;
    uint64_t record_count = 0;
    uint64_t time_temp;
    while (!force_quit/* && loop_count < 1000*/ && record_count < MAX_RECORD_COUNT) {
        int i;
        for (i = 0; i < lconf->n_rx_queue; i++){
            const uint16_t nb_rx = rte_eth_rx_burst(lconf->port, lconf->rx_queue_list[i], bufs, BURST_SIZE);
            if(nb_rx != 0){
            // struct rte_udp_hdr *udp_h = rte_pktmbuf_mtod_offset(bufs[0], struct rte_udp_hdr *, 34);
            // printf("rss hash value: %d,pkt_len:%d,dst_port:%d,src_port:%d\n",
            //     bufs[0]->hash.rss,
            //     bufs[0]->data_len,
            //     udp_h->dst_port,
            //     udp_h->src_port);
                total_rx += nb_rx;
                length += (bufs[0]->data_len*nb_rx);
                rte_pktmbuf_free_bulk(bufs, nb_rx);
            }
        }
        
        loop_count++;
        time_temp = rte_rdtsc();
        double time_inter_temp=(double)(time_temp-time_last_print)/rte_get_timer_hz();
        if (time_inter_temp>=0.5){
            time_last_print=time_temp;
            drx = total_rx - last_total_rx;
            last_total_rx = total_rx;
            dlength = length - last_length;
            last_length = length;
            // APP_LOG("lcoreID %ld: run time: %lf.\n", lcore_id, (time_temp-(double)start)/rte_get_timer_hz());
            // APP_LOG("lcoreID %ld: during this time, send %ld pkts, throughput: %lf pps, %lf bps.\n", lcore_id,dtx, (double)dtx/time_inter_temp, (double)dlength/time_inter_temp);
            // APP_LOG("times of loop is %ld, should send packets %ld.\n", loop_count, loop_count);
            rx_pps_timeline[lcore_id][record_count] = (double)drx/time_inter_temp;
            rx_bps_timeline[lcore_id][record_count] = (double)dlength*8/time_inter_temp;
            // timeline[lcore_id][record_count] = (double)(time_temp-start)/rte_get_timer_hz();
            record_count++;
        }
    }
    double time_interval = (double)(rte_rdtsc() - start)/rte_get_timer_hz();
    APP_LOG("run time: %lf.\n", time_interval);
    APP_LOG("Sent %ld pkts, received %ld pkts, throughput: %lf pps, %lf bps.\n", total_tx, total_rx, (double)total_rx/time_interval, (double)length*8/time_interval);
    APP_LOG("times of loop is %ld, should send packets %ld.\n", loop_count, loop_count*32);
    // tx_pkt_num[lcore_id] = total_tx;
    rx_pkt_num[lcore_id] = total_rx;
    rx_pps[lcore_id] = (double)total_rx/time_interval;
    rx_bps[lcore_id] = (double)length*8/time_interval;
}


static int
launch_one_lcore(__attribute__((unused)) void *arg){
    uint32_t lcore_id = rte_lcore_id();
	lcore_main(lcore_id);
	return 0;
}

/* Main functional part of port initialization. 8< */
static inline int
port_init(uint16_t port, uint32_t *n_lcores_p)
{
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
    *n_lcores_p = n_lcores;

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
        printf("create mbuf pool\n");
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
    int i, j;
    uint32_t n_lcores = 0;

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

    /* Initialize the ports. */
    if (port_init(enabled_port, &n_lcores) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %u\n", enabled_port);

    /* Calculate minimum TSC diff for returning signed packets */
    min_txintv *= rte_get_timer_hz() / US_PER_S;
    printf("TSC freq: %lu Hz\n", rte_get_timer_hz());

    /* Call lcore_main on the main core only. */
    printf("core_num:%d\n",n_lcores);
    rte_eal_mp_remote_launch((lcore_function_t *)launch_one_lcore, NULL, CALL_MAIN);
    RTE_LCORE_FOREACH_WORKER(lcore_id){
        if (rte_eal_wait_lcore(lcore_id) < 0) {
            ret = -1;
            break;
        } 
    }

    /*output statics*/
    uint64_t total_tx_pkt_num = 0, total_rx_pkt_num = 0;
    double total_rx_pps = 0.0, total_rx_bps = 0.0;
    FILE *fp;
    struct timeval timetag;

    gettimeofday(&timetag, NULL);
    if (unlikely(access("../lab_results/pkt_rcv_mul_auto_sta/throughput.csv", 0) != 0)){
        fp = fopen("../lab_results/pkt_rcv_mul_auto_sta/throughput.csv", "a+");
        fprintf(fp, "core,timestamp,send_pkts,rcv_pkts,send_pps,send_bps,rcv_pps,rcv_bps\r\n");
    }else{
        fp = fopen("../lab_results/pkt_rcv_mul_auto_sta/throughput.csv", "a+");
    }

    for(i = 0; i < MAX_LCORES; i++){
        total_tx_pkt_num += tx_pkt_num[i];
        total_rx_pkt_num +=rx_pkt_num[i];
        total_rx_pps += rx_pps[i];
        total_rx_bps += rx_bps[i];
    }
    APP_LOG("Total Sent %ld pkts, received %ld pkts, throughput: %lf pps, %lf bps.\n", total_tx_pkt_num, total_rx_pkt_num, total_rx_pps, total_rx_bps);
    fprintf(fp, "%d,%ld,%ld,%ld,0,0,%lf,%lf\r\n", n_lcores, timetag.tv_sec, total_tx_pkt_num, total_rx_pkt_num, total_rx_pps, total_rx_bps);
    fclose(fp);

    double to_print;
    if (unlikely(access("../lab_results/pkt_rcv_mul_auto_sta/throughput_pps.csv", 0) != 0)){
        fp = fopen("../lab_results/pkt_rcv_mul_auto_sta/throughput_pps.csv", "a+");
        fprintf(fp, "core,timestamp,time,rcv_pps\r\n");
    }else{
        fp = fopen("../lab_results/pkt_rcv_mul_auto_sta/throughput_pps.csv", "a+");
    }
    for (i = 0;i<MAX_RECORD_COUNT;i++){
        to_print = 0;
        for (j = 0;j<MAX_LCORES;j++){
            to_print += rx_pps_timeline[j][i];
        }
        fprintf(fp, "%d,%ld,%d,%lf\r\n", n_lcores, timetag.tv_sec, i, to_print);
    }
    fclose(fp);

    if (unlikely(access("../lab_results/pkt_rcv_mul_auto_sta/throughput_bps.csv", 0) != 0)){
        fp = fopen("../lab_results/pkt_rcv_mul_auto_sta/throughput_bps.csv", "a+");
        fprintf(fp, "core,timestamp,time,rcv_bps\r\n");
    }else{
        fp = fopen("../lab_results/pkt_rcv_mul_auto_sta/throughput_bps.csv", "a+");
    }

    for (i = 0;i<MAX_RECORD_COUNT;i++){
        to_print = 0;
        for (j = 0;j<MAX_LCORES;j++){
            to_print += rx_bps_timeline[j][i];
        }
        fprintf(fp, "%d,%ld,%d,%lf\r\n", n_lcores, timetag.tv_sec, i, to_print);
    }
    fclose(fp);

    /* Cleaning up. */
    fflush(stdout);
APP_RTE_CLEANUP:
    printf("Closing port %u... ", enabled_port);
    rte_eth_dev_stop(enabled_port);
    rte_eth_dev_close(enabled_port);
    printf("Done.\n");

    return 0;
}
