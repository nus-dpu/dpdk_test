#include <stdint.h>
#include <stdlib.h>
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
#include <rte_memcpy.h>
#include <getopt.h>
#include <unistd.h> //For sleep()
#include <math.h> //For pow()
#include <sys/time.h>
#include <pcap.h>

#include "para.h"

#define MAX_LCORES 72
#define RX_QUEUE_PER_LCORE   1
#define TX_QUEUE_PER_LCORE   1
#define BURST_SIZE 32
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define NUM_MBUFS 4095//(PKTS_NUM+BURST_SIZE)
#define MBUF_SIZE  (1518+sizeof(struct rte_mbuf)+RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 32
#define PAY_LOAD_LEN (PKT_LEN-28) //udp 
// #define PCAP_ENABLE

#define APP_LOG(...) RTE_LOG(INFO, USER1, __VA_ARGS__)
// #define PRN_COLOR(str) ("\033[0;33m" str "\033[0m")	// Yellow accent

#define TO_STRING(a) #a
#define STRING_THRANFER(a) TO_STRING(a)

#define SRC_IP_PREFIX ((192<<24)) 
#define DEST_IP_PREFIX ((193<<24)) 

#define META_OFF (sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_udp_hdr))
#define IPV4_OFF (sizeof(struct rte_ether_hdr))


#define THROUGHPUT_FILE "../lab_results/" PROGRAM "/throughput.csv"
#define THROUGHPUT_TIME_FILE   "../lab_results/" PROGRAM "/throughput_time.csv"
#define RTT_FILE   "../lab_results/" PROGRAM "/pkt_rtt.csv"

#ifdef PCAP_ENABLE
#define PCAP_FILE(a) ("../../dataset/synthetic/flow_" STRING_THRANFER(a) ".pcap")
#endif

struct lcore_configuration {
    uint32_t vid; // virtual core id
    uint32_t port; // one port
    uint32_t n_tx_queue;  // number of TX queues
    uint32_t tx_queue_list[TX_QUEUE_PER_LCORE]; // list of TX queues
    uint32_t n_rx_queue;  // number of RX queues
    uint32_t rx_queue_list[RX_QUEUE_PER_LCORE]; // list of RX queues
} __rte_cache_aligned;

struct flow {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
};

struct flow_info{
    uint64_t flow_size;
    uint64_t pkt_seq;
};

struct packet_buffer {
    struct rte_mbuf **mbufs;
    uint32_t count;
    uint32_t lcore_id;
    uint32_t queue_id;
};

struct meta_data {
    uint64_t flow_size;
    uint64_t pkt_seq;
    struct timeval time;
};

struct flow_log {
    double tx_pps_t;
    double tx_bps_t;
    double rx_pps_t;
    double rx_bps_t;
};

struct rtt_info
{
    uint32_t flow_id; //now src_ip
    uint64_t flow_size;
    uint64_t pkt_seq;
    uint64_t rtt;
};

struct rte_ether_addr src_mac;
struct rte_ether_addr dst_mac;

static unsigned enabled_port = 0;
static uint64_t min_txintv = 15/*us*/;	// min cycles between 2 returned packets
static volatile bool force_quit = false;
static volatile uint16_t nb_pending = 0;

pcap_t *handle;
struct lcore_configuration lcore_conf[MAX_LCORES];
struct rte_mempool *pktmbuf_pool[MAX_LCORES*RX_QUEUE_PER_LCORE];
struct packet_buffer pkt_buffer[MAX_LCORES*RX_QUEUE_PER_LCORE];

uint64_t tx_pkt_num[MAX_LCORES];
uint64_t rx_pkt_num[MAX_LCORES];
double tx_pps[MAX_LCORES];
double rx_pps[MAX_LCORES];
double tx_bps[MAX_LCORES];
double rx_bps[MAX_LCORES];
struct flow_log *flowlog_timeline[MAX_LCORES];
struct rtt_info *rtt_data = NULL;

static inline void
fill_ethernet_header(struct rte_ether_hdr *eth_hdr) {
	struct rte_ether_addr s_addr = src_mac; 
	struct rte_ether_addr d_addr = dst_mac;
	eth_hdr->src_addr =s_addr;
	eth_hdr->dst_addr =d_addr;
	eth_hdr->ether_type = rte_cpu_to_be_16(0x0800);
}

static inline void
fill_ipv4_header(struct rte_ipv4_hdr *ipv4_hdr, const struct flow *flow_id) {
	ipv4_hdr->version_ihl = (4 << 4) + 5; // ipv4, length 5 (*4)
	ipv4_hdr->type_of_service = 0; // No Diffserv
	ipv4_hdr->total_length = rte_cpu_to_be_16(PKT_LEN); // tcp 20
	ipv4_hdr->packet_id = rte_cpu_to_be_16(5462); // set random
	ipv4_hdr->fragment_offset = rte_cpu_to_be_16(0);
	ipv4_hdr->time_to_live = 64;
	ipv4_hdr->next_proto_id = 17; // udp
	ipv4_hdr->hdr_checksum = rte_cpu_to_be_16(0x0);

    ipv4_hdr->src_addr = rte_cpu_to_be_32(flow_id->src_ip); 
	ipv4_hdr->dst_addr = rte_cpu_to_be_32(flow_id->dst_ip);

	ipv4_hdr->hdr_checksum = rte_cpu_to_be_16(rte_ipv4_cksum(ipv4_hdr));
}

static inline void
fill_udp_header(struct rte_udp_hdr *udp_hdr, struct rte_ipv4_hdr *ipv4_hdr, const struct flow *flow_id) {
    udp_hdr->src_port = rte_cpu_to_be_16(flow_id->src_port);
	udp_hdr->dst_port = rte_cpu_to_be_16(flow_id->dst_port);
	udp_hdr->dgram_len = rte_cpu_to_be_16(PKT_LEN - sizeof(struct rte_ipv4_hdr));
    udp_hdr->dgram_cksum = rte_cpu_to_be_16(0x0);
	
	udp_hdr->dgram_cksum = rte_cpu_to_be_16(rte_ipv4_udptcp_cksum(ipv4_hdr, udp_hdr));
}

// static inline void
// fill_tcp_header(struct rte_tcp_hdr *tcp_hdr, struct rte_ipv4_hdr *ipv4_hdr) {
// 	tcp_hdr->src_port = rte_cpu_to_be_16(0x162E);
// 	tcp_hdr->dst_port = rte_cpu_to_be_16(0x04d2);
// 	tcp_hdr->sent_seq = rte_cpu_to_be_32(0);
// 	tcp_hdr->recv_ack = rte_cpu_to_be_32(0);
// 	tcp_hdr->data_off = 0;
// 	tcp_hdr->tcp_flags = 0;
// 	tcp_hdr->rx_win = rte_cpu_to_be_16(16);
// 	tcp_hdr->cksum = rte_cpu_to_be_16(0x0);
// 	tcp_hdr->tcp_urp = rte_cpu_to_be_16(0);

// 	tcp_hdr->cksum = rte_cpu_to_be_16(rte_ipv4_udptcp_cksum(ipv4_hdr, tcp_hdr));
// }

static inline void
fill_payload(struct meta_data *meta, const struct flow_info *info) {
    meta->flow_size = info->flow_size;
    meta->pkt_seq = info->pkt_seq;
}

static struct rte_mbuf *make_testpkt(uint32_t queue_id, const struct flow *flow, const struct flow_info *info)
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
	fill_ipv4_header(ipv4_h, flow);
    curr_ofs += sizeof(struct rte_ipv4_hdr);

    // struct rte_tcp_hdr *tcp_h = rte_pktmbuf_mtod_offset(mp, struct rte_tcp_hdr *, curr_ofs);
	// fill_tcp_header(tcp_h, ipv4_h);
    // curr_ofs += sizeof(struct rte_tcp_hdr);

    struct rte_udp_hdr *udp_h = rte_pktmbuf_mtod_offset(mp, struct rte_udp_hdr *, curr_ofs);
	fill_udp_header(udp_h, ipv4_h,flow);
    curr_ofs += sizeof(struct rte_udp_hdr);
    
    struct meta_data * payload_data= rte_pktmbuf_mtod_offset(mp, struct meta_data *, curr_ofs);
    fill_payload(payload_data, info);

    return mp;
}

#ifdef PCAP_ENABLE
// Callback function to process each packet in pcap
void packet_handler(uint8_t *args, const struct pcap_pkthdr *header, const uint8_t *packet)
{
    struct packet_buffer *buffer = (struct packet_buffer *)args;
    uint32_t queue_id = buffer->queue_id;

    // Allocate an mbuf for the packet
    rte_memcpy(rte_pktmbuf_mtod(buffer->mbufs[buffer->count], void *), packet, header->caplen);
    buffer->mbufs[buffer->count]->pkt_len = header->len;
    buffer->mbufs[buffer->count]->data_len = header->caplen;
    buffer->count++;

    // int a;
    // printf("packet %d:\n", buffer->count);
    // // uint8_t* pkt_p = (uint8_t*)rte_pktmbuf_mtod(buffer->mbufs[buffer->count], void *);
    // for(a = 0; a < 78; a++){
    //     printf("%02x ", packet[a]);
    //     if(a % 16 == 15){
    //         printf("yes");
    //         printf("\n");
    //     }
    // }
    // printf("\n");
}
#endif

void int_to_string(int input, char* output){
    sprintf(output, "%d", input);
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

    /* Run until the application is quit or killed. */
    struct lcore_configuration *lconf = &lcore_conf[lcore_id];
    uint32_t queue_id;
    struct rte_mbuf *bufs_rx[BURST_SIZE], *bufs_tx[BURST_SIZE];
    
    int pkt_batch_count = 0;
    
    uint64_t send_pkt = 0, rcv_pkt = 0;
    uint64_t loop_count = 0;
    uint64_t record_count = 0;
    /* pkts_sta */
    uint64_t total_tx = 0, total_rx = 0;
    uint64_t txB[BURST_SIZE], rxB[BURST_SIZE];
    uint64_t total_txB = 0, total_rxB = 0;
    uint64_t last_total_tx = 0, last_total_rx = 0;
    uint64_t last_total_txB = 0, last_total_rxB = 0;
    // uint64_t total_txb = 0, total_rxb = 0; //we send same length packets now, can be ignored
    /* time_sta */
    uint64_t start = rte_rdtsc();
    uint64_t time_last_print = start;
    uint64_t time_now;
    struct timeval tv;

    if (unlikely(flowlog_timeline[lcore_id] != NULL)){
        rte_exit(EXIT_FAILURE, "There are error when allocate memory to flowlog.\n");
    }else{
        flowlog_timeline[lcore_id] = (struct flow_log *) malloc(sizeof(struct flow_log) * MAX_RECORD_COUNT);
        memset(flowlog_timeline[lcore_id], 0, sizeof(struct flow_log) * MAX_RECORD_COUNT);
    }

    if (rtt_data == NULL){
        rtt_data = (struct rtt_info *) malloc(sizeof(struct rtt_info) * PKTS_NUM);; //not thread safety
        memset(rtt_data, 0, sizeof(struct rtt_info) * PKTS_NUM);
    }

    int i, j;
    #ifdef PCAP_ENABLE
    for (i = 0; i < lconf->n_rx_queue; i++){
        queue_id = lconf->rx_queue_list[i];
        pkt_buffer[queue_id].mbufs = (struct rte_mbuf **) malloc(sizeof(struct rte_mbuf *) * PKTS_NUM);
        //initialize pkt_buffer
        int num_allocated = rte_pktmbuf_alloc_bulk(pktmbuf_pool[queue_id], 
                               pkt_buffer[queue_id].mbufs, PKTS_NUM);
        pkt_buffer[queue_id].count = 0;
        pkt_buffer[queue_id].lcore_id = lcore_id;
        pkt_buffer[queue_id].queue_id = queue_id;
        if(pcap_loop(handle, -1, packet_handler, (u_char *)&pkt_buffer[queue_id]) != 0){
            rte_exit(EXIT_FAILURE, "Error in pcap_loop: %s\n", pcap_geterr(handle));
        }
    }
    #endif

    printf("Core %u forwarding packets. [Ctrl+C to quit]\n", rte_lcore_id());
    fflush(stdout);

    while (!force_quit && record_count < MAX_RECORD_COUNT) {
        for (i = 0; i < lconf->n_rx_queue; i++){
            #ifdef PCAP_ENABLE
            rte_pktmbuf_alloc_bulk(pktmbuf_pool[queue_id], 
                                   bufs_tx, 
                                   BURST_SIZE);
            #endif
            // Transmit packet
            if (send_pkt <= PKTS_NUM){
                for (j = 0; j < BURST_SIZE; j++){
                    struct flow flow_id;
                    struct flow_info flow_info;

                    flow_id.src_ip = SRC_IP_PREFIX + (uint32_t)(send_pkt % FLOW_NUM);
                    flow_id.dst_ip = ((192<<24) + (168<<16) + (200<<8)) + 1;
                    flow_id.src_port = 1234;
                    flow_id.dst_port = 4321;

                    flow_info.flow_size = FLOW_SIZE;
                    flow_info.pkt_seq = (send_pkt / FLOW_NUM) + 1;
                    
                    #ifndef PCAP_ENABLE
                    bufs_tx[j] = make_testpkt(lconf->rx_queue_list[i], &flow_id, &flow_info);
                    #endif

                    /* packet copy from buffer to send*/
                    // rte_memcpy(rte_pktmbuf_mtod(bufs_tx[j], void *), 
                    //            rte_pktmbuf_mtod(pkt_buffer[queue_id].mbufs[send_pkt], void*), 
                    //            pkt_buffer[queue_id].mbufs[send_pkt]->data_len); 
                    // bufs_tx[j]->pkt_len = pkt_buffer[queue_id].mbufs[send_pkt]->pkt_len;
                    // bufs_tx[j]->data_len = pkt_buffer[queue_id].mbufs[send_pkt]->data_len;
                    
                    txB[j] = bufs_tx[j]->data_len;
                    send_pkt++;
                }

                gettimeofday(&tv, NULL);
                for (j = 0; j < BURST_SIZE; j++){
                    struct meta_data *meta = rte_pktmbuf_mtod_offset(bufs_tx[j], struct meta_data *, META_OFF);
                    meta->time.tv_sec = tv.tv_sec;
                    meta->time.tv_usec = tv.tv_usec;
                }
                uint16_t nb_tx = rte_eth_tx_burst(lconf->port, lconf->tx_queue_list[i], bufs_tx, BURST_SIZE);
                total_tx += nb_tx;
                for (j = 0; j < nb_tx; total_txB += txB[j], j++)
                if (nb_tx < BURST_SIZE){
                    rte_pktmbuf_free_bulk(bufs_tx + nb_tx, BURST_SIZE - nb_tx);
                }
            }

            //Receive packets
            const uint16_t nb_rx = rte_eth_rx_burst(lconf->port, lconf->rx_queue_list[i], bufs_rx, BURST_SIZE);
            gettimeofday(&tv, NULL);
            total_rx += nb_rx;
            for (j = 0; j < nb_rx; j++){
                struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(bufs_rx[j], struct rte_ipv4_hdr *, IPV4_OFF);
                struct meta_data *meta = rte_pktmbuf_mtod_offset(bufs_rx[j], struct meta_data *, META_OFF);
                uint64_t rtt = (tv.tv_sec - meta->time.tv_sec) * 1e6 + (tv.tv_usec - meta->time.tv_usec); //print microsecond

                rtt_data[rcv_pkt].flow_id = ipv4_hdr->src_addr - SRC_IP_PREFIX; //now src_ip, change when flow format change
                rtt_data[rcv_pkt].flow_size = meta->flow_size;
                rtt_data[rcv_pkt].pkt_seq = meta->pkt_seq;
                rtt_data[rcv_pkt].rtt = rtt;
                rcv_pkt ++;

                total_txB += bufs_rx[j]->data_len;
            }

            if (nb_rx > 0){
                rte_pktmbuf_free_bulk(bufs_rx, nb_rx);
            }
        }
        loop_count++;

        //save log
        time_now = rte_rdtsc();
        double time_inter_temp=(double)(time_now-time_last_print)/rte_get_timer_hz();
        if (time_inter_temp >= 0.5){
            uint64_t dtx, drx;
            uint64_t dtxB, drxB;

            dtx = total_tx - last_total_tx;
            drx = total_rx - last_total_rx;
            dtxB = total_txB - last_total_txB;
            drxB = total_rxB - last_total_rxB;

            time_last_print = time_now;
            last_total_tx = total_tx;
            last_total_txB = total_txB;
            last_total_rx = total_rx;
            last_total_rxB = total_rxB;
            flowlog_timeline[lcore_id][record_count].tx_pps_t = (double)dtx/time_inter_temp;
            flowlog_timeline[lcore_id][record_count].tx_bps_t = (double)dtxB*8/time_inter_temp;
            flowlog_timeline[lcore_id][record_count].rx_pps_t = (double)drx/time_inter_temp;
            flowlog_timeline[lcore_id][record_count].rx_bps_t = (double)drxB*8/time_inter_temp;
          record_count++;
        }
    }
    double time_interval = (double)(rte_rdtsc() - start)/rte_get_timer_hz();
    APP_LOG("lcoreID %d: run time: %lf.\n", lcore_id, time_interval);
    APP_LOG("lcoreID %d: Sent %ld pkts, received %ld pkts; TX: %lf pps, %lf bps; RX: %lf pps, %lf bps.\n", \
            lcore_id, total_tx, total_rx, (double)total_tx/time_interval, (double) total_txB*8/time_interval, \
            (double)total_rx/time_interval, (double) total_rxB*8/time_interval);
    APP_LOG("lcoreID %d: times of loop is %ld, should send packets %ld.\n", lcore_id, loop_count, loop_count*BURST_SIZE);
    tx_pkt_num[lcore_id] = total_tx;
    rx_pkt_num[lcore_id] = total_rx;
    tx_pps[lcore_id] = (double)total_tx/time_interval;
    rx_pps[lcore_id] = (double)total_rx/time_interval;
    tx_bps[lcore_id] = (double)total_txB*8/time_interval;
    rx_bps[lcore_id] = (double)total_rxB*8/time_interval;
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
port_init(uint16_t port, uint32_t *n_lcores_p)
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

#ifdef PCAP_ENABLE
static int pcap_init(void){
    //open pcap file
    const char *pcap_file = PCAP_FILE(FLOW_NUM);
    char errbuf[PCAP_ERRBUF_SIZE];
    handle = pcap_open_offline(pcap_file, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Cannot open PCAP file: %s\n", errbuf);
        return 1;
    }
    // // Set the interface to promiscuous mode
    // if (pcap_set_promisc(handle, 1) != 0)
    // {
    //     fprintf(stderr,  "Failed to set promiscuous mode\n");
    //     return 1;
    // }
    return 0;
}
#endif

int main(int argc, char *argv[])
{
    unsigned nb_ports;
    unsigned lcore_id;
    uint32_t n_lcores = 0;
    int i,j;    
    struct timeval timetag;

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
    if (port_init(enabled_port, &n_lcores) != 0)
        rte_exit(EXIT_FAILURE, "Cannot init port %u\n", enabled_port);
    
    printf("core_num:%d\n",n_lcores);

    #ifdef PCAP_ENABLE
    /* open pcap file */
    if (pcap_init() != 0)
        rte_exit(EXIT_FAILURE, "Cannot correctly open pcap file\n");
    #endif
    gettimeofday(&timetag, NULL);
    rte_eal_mp_remote_launch((lcore_function_t *)launch_one_lcore, NULL, CALL_MAIN);
    RTE_LCORE_FOREACH_WORKER(lcore_id){
        if (rte_eal_wait_lcore(lcore_id) < 0) {
            ret = -1;
            break;
        } 
    }

    #ifdef PCAP_ENABLE
    pcap_close(handle);
    #endif


    uint64_t total_tx_pkt_num = 0, total_rx_pkt_num = 0;
    double total_tx_pps = 0.0, total_tx_bps = 0.0;
    double total_rx_pps = 0.0, total_rx_bps = 0.0;
    FILE *fp;

    if (unlikely(access(THROUGHPUT_FILE, 0) != 0)){
        fp = fopen(THROUGHPUT_FILE, "a+");
        if(unlikely(fp == NULL)){
            rte_exit(EXIT_FAILURE, "Cannot open file %s\n", THROUGHPUT_FILE);
        }
        fprintf(fp, "core,timestamp,flow_num,pkt_len,send_pkts,rcv_pkts,send_pps,send_bps,rcv_pps,rcv_bps\r\n");
    }else{
        fp = fopen(THROUGHPUT_FILE, "a+");
    }
    for(i = 0; i < MAX_LCORES; i++){
        total_tx_pkt_num += tx_pkt_num[i];
        total_rx_pkt_num +=rx_pkt_num[i];
        total_tx_pps += tx_pps[i];
        total_tx_bps += tx_bps[i];
        total_rx_pps += rx_pps[i],
        total_rx_bps += rx_bps[i];
    }
    fprintf(fp, "%d,%ld,%d,%d,%ld,%ld,%lf,%lf,%lf,%lf\r\n", \
            n_lcores, timetag.tv_sec, FLOW_NUM, PKT_LEN, total_tx_pkt_num, total_rx_pkt_num, \
            total_tx_pps, total_tx_bps, total_rx_pps, total_rx_bps);
    fclose(fp);
    APP_LOG("Total Sent %ld pkts, received %ld pkts; TX: %lf pps, %lf bps; RX: %lf pps, %lf bps.\n", \
            total_tx_pkt_num, total_rx_pkt_num, total_tx_pps, total_tx_bps, total_rx_pps, total_rx_bps);
 
    if (unlikely(access(THROUGHPUT_TIME_FILE, 0) != 0)){
        fp = fopen(THROUGHPUT_TIME_FILE, "a+");
        fprintf(fp, "core,timestamp,flow_num,pkt_len,time, \
                     send_pps,send_bps,rcv_pps,rcv_bps\r\n");
    }else{
        fp = fopen(THROUGHPUT_TIME_FILE, "a+");
    }
    for (i = 0;i<MAX_RECORD_COUNT;i++){
        double tx_pps_p = 0, tx_bps_p = 0;
        double rx_pps_p = 0, rx_bps_p = 0;
        for (j = 0;j<MAX_LCORES;j++){
            if(rte_lcore_is_enabled(j)) {
                tx_pps_p += flowlog_timeline[j][i].tx_pps_t;
                tx_bps_p += flowlog_timeline[j][i].tx_bps_t;
                rx_pps_p += flowlog_timeline[j][i].rx_pps_t;
                rx_bps_p += flowlog_timeline[j][i].rx_bps_t;
            }
        }
        fprintf(fp, "%d,%ld,%d,%d,%d,%lf,%lf,%lf,%lf\r\n", \
                n_lcores, timetag.tv_sec, FLOW_NUM, PKT_LEN, i, \
                tx_pps_p,tx_bps_p,rx_pps_p,rx_bps_p);
    }
    fclose(fp);

    if (unlikely(access(RTT_FILE, 0) != 0)){
        fp = fopen(RTT_FILE, "a+");
        fprintf(fp, "flow_id, flow_size, pkt_seq, rtt\r\n");
    }else{
        fp = fopen(RTT_FILE, "a+");
    }
    for (i = 0; i < PKTS_NUM ; i++){
        fprintf(fp, "%d,%ld,%ld,%ld\r\n", \
                rtt_data[i].flow_id, \
                rtt_data[i].flow_size, \
                rtt_data[i].pkt_seq, \
                rtt_data[i].rtt);
    }
    fclose(fp);

    for (j = 0;j<MAX_LCORES;j++){
        if(rte_lcore_is_enabled(j)) {
            free(flowlog_timeline[j]);
        }
    }
    free(rtt_data);
    /* Cleaning up. */
    fflush(stdout);
APP_RTE_CLEANUP:
    printf("Closing port %u... ", enabled_port);
    rte_eth_dev_stop(enabled_port);
    rte_eth_dev_close(enabled_port);
    printf("Done.\n");

    return 0;
}

