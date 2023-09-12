/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <unistd.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_net.h>
#include <rte_flow.h>
#include <rte_cycles.h>

#include "flow_blocks.c"
#include "packet_make.c"

#include "para.h"

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define MBUF_SIZE   (10000+sizeof(struct rte_mbuf)+RTE_PKTMBUF_HEADROOM)

#define MAX_LCORES 72
#define RX_QUEUE_PER_LCORE   1
#define TX_QUEUE_PER_LCORE   1
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define SRC_IP ((0<<24) + (0<<16) + (0<<8) + 0) /* src ip = 0.0.0.0 */
#define DEST_IP_PREFIX ((192<<24)) /* dest ip prefix = 192.0.0.0.0 */
#define FULL_MASK 0xffffffff /* full mask */
#define EMPTY_MASK 0x0 /* empty mask */

#define BURST_SIZE 32

struct lcore_configuration {
    uint32_t vid; // virtual core id
    uint32_t port; // one port
    uint32_t n_tx_queue;  // number of TX queues
    uint32_t tx_queue_list[TX_QUEUE_PER_LCORE]; // list of TX queues
    uint32_t n_rx_queue;  // number of RX queues
    uint32_t rx_queue_list[RX_QUEUE_PER_LCORE]; // list of RX queues
} __rte_cache_aligned;

static volatile bool force_quit;

static uint16_t port_id;
static uint16_t nr_queues = 5;
struct rte_mempool *mbuf_pool;
struct rte_flow *flow;

struct lcore_configuration lcore_conf[MAX_LCORES];
struct rte_mempool *pktmbuf_pool[MAX_LCORES];

/* Main_loop for flow filtering. 8< */
static int
packet_send_main_loop(void)
{
	struct rte_flow_error error;
	uint16_t nb_tx;
	// uint64_t total_tx, length, loop_count;
	uint16_t i;
	int ret;
	struct rte_mbuf *bufs_tx[BURST_SIZE];

	/* Reading the packets from all queues. 8< */
	// while (!force_quit) {
    //     for(i = 0; i < BURST_SIZE; i++){
    //         bufs_tx[i] = make_testpkt(mbuf_pool);
	// 		if (bufs_tx[i] == NULL)
	// 		    printf("Error: failed to allocate packet buffer\n");
    //     }
    //     // Transmit packet
    //     nb_tx = rte_eth_tx_burst(port_id, 0, bufs_tx, BURST_SIZE);
    //     // total_tx += nb_tx;
    //     // length += (bufs_tx[0]->data_len*nb_tx);
	// 	// loop_count++;
    //     if (nb_tx < BURST_SIZE){
    //         rte_pktmbuf_free_bulk(bufs_tx + nb_tx, BURST_SIZE - nb_tx);
    //     }
	// }
	/* >8 End of reading the packets from all queues. */

	/* closing and releasing resources */
	rte_flow_flush(port_id, &error);
	ret = rte_eth_dev_stop(port_id);
	if (ret < 0)
		printf("Failed to stop port %u: %s",
		       port_id, rte_strerror(-ret));
	rte_eth_dev_close(port_id);
	return ret;
}
/* >8 End of main_loop for flow filtering. */

#define CHECK_INTERVAL 1000  /* 100ms */
#define MAX_REPEAT_TIMES 90  /* 9s (90 * 100ms) in total */

static void lcore_main(uint32_t lcore_id){
	struct rte_flow_error error;
	FILE *fp;
	double *time_list = NULL;
	double *rte_time_list = NULL;
	time_list = (double *)malloc(sizeof(double)*TEST_FLOW_NUM);
	rte_time_list = (double *)malloc(sizeof(double)*TEST_FLOW_NUM);

	if (time_list == NULL || rte_time_list == NULL){
		printf("memory for time_list allocate fail\n");
	}
	/* Create flow for send packet with. 8< */    
	int i;
	for(i = 0 ; i < BASE_FLOW_NUM; i++){
		double rte_time = 0;
		flow = generate_ipv4_udp_flow(port_id, 0, SRC_IP, FULL_MASK, 
		                              DEST_IP_PREFIX + i, FULL_MASK, 
									  1234, 5678, &error, &rte_time);
		if (!flow) {
			printf("Flow can't be created %d message: %s\n",
			       error.type,
			       error.message ? error.message : "(no stated reason)");
			break;
		}
	}

    printf("already add %d flows\n", BASE_FLOW_NUM);

	for(i = 0 ; i < TEST_FLOW_NUM; i++){
		double rte_time = 0;
		uint64_t start = rte_rdtsc();
		flow = generate_ipv4_udp_flow(port_id, 0, SRC_IP, FULL_MASK, 
		                              DEST_IP_PREFIX + i + BASE_FLOW_NUM, FULL_MASK, 
									  1234, 5678, &error, &rte_time);
		double add_time=(double)(rte_rdtsc() - start) / rte_get_timer_hz();
		time_list[i] = add_time;
		rte_time_list[i] = rte_time;
		if (!flow) {
			printf("Flow can't be created %d message: %s\n",
			       error.type,
			       error.message ? error.message : "(no stated reason)");
			break;
		}
		// else{
		// 	printf("already add %d flows, add time is %lf\n", i+1, add_time);
		// }
	}

    double time_all = 0;
	double rte_time_all = 0;
	if (unlikely(access(RUN_TIME_FILE, 0) != 0)){
        fp = fopen(RUN_TIME_FILE, "a+");
        fprintf(fp, "base_flow_num,test_flow_num,index,run_time,rte_create_time\r\n");
    }else{
        fp = fopen(RUN_TIME_FILE, "a+");
    }
	if( fp == NULL ){
		printf("failed to open the file\n");
	}
	for(i = 0 ; i < TEST_FLOW_NUM; i++){
		fprintf(fp,"%d,%d,%d,%lf,%lf\r\n", BASE_FLOW_NUM, TEST_FLOW_NUM, i+1, time_list[i], rte_time_list[i]);
		time_all = time_all + time_list[i];
		rte_time_all = rte_time_all + rte_time_list[i];
	}
	fclose(fp);

	if (unlikely(access(AVG_TIME_FILE, 0) != 0)){
        fp = fopen(AVG_TIME_FILE, "a+");
        fprintf(fp, "base_flow_num, test_flow_num, run_time, rte_create_time\r\n");
    }else{
        fp = fopen(AVG_TIME_FILE, "a+");
    }
	if( fp == NULL ){
		printf("failed to open the file\n");
	}
	
	fprintf(fp,"%d,%d,%lf,%lf\r\n", BASE_FLOW_NUM, TEST_FLOW_NUM, (time_all/TEST_FLOW_NUM), (rte_time_all/TEST_FLOW_NUM));
	fclose(fp);

	free(time_list);
	free(rte_time_list);

	printf("finish file writing\n");
}

static int
launch_one_lcore(__attribute__((unused)) void *arg){
    uint32_t lcore_id = rte_lcore_id();
	lcore_main(lcore_id);
	return 0;
}

static void
assert_link_status(void)
{
	struct rte_eth_link link;
	uint8_t rep_cnt = MAX_REPEAT_TIMES;
	int link_get_err = -EINVAL;

	memset(&link, 0, sizeof(link));
	do {
		link_get_err = rte_eth_link_get(port_id, &link);
		if (link_get_err == 0 && link.link_status == ETH_LINK_UP)
			break;
		rte_delay_ms(CHECK_INTERVAL);
	} while (--rep_cnt);

	if (link_get_err < 0)
		rte_exit(EXIT_FAILURE, ":: error: link get is failing: %s\n",
			 rte_strerror(-link_get_err));
	if (link.link_status == ETH_LINK_DOWN)
		rte_exit(EXIT_FAILURE, ":: error: link is still down\n");
}

/* Port initialization used in flow filtering. 8< */
static void
init_port(uint32_t *n_lcores_p)
{
	int ret;
	uint16_t i,j;
	uint32_t n_lcores = 0;
	uint16_t rx_rings = 0, tx_rings = 0;
	uint16_t nb_rxd = RX_RING_SIZE;
	uint16_t nb_txd = TX_RING_SIZE;

	/* Ethernet port configured with default settings. 8< */
	struct rte_eth_conf port_conf = {
		.rxmode = {
			.split_hdr_size = 0,
		},
		.txmode = {
			.offloads =
				DEV_TX_OFFLOAD_VLAN_INSERT |
				DEV_TX_OFFLOAD_IPV4_CKSUM  |
				DEV_TX_OFFLOAD_UDP_CKSUM   |
				DEV_TX_OFFLOAD_TCP_CKSUM   |
				DEV_TX_OFFLOAD_SCTP_CKSUM  |
				DEV_TX_OFFLOAD_TCP_TSO,
		},
	};
	struct rte_eth_txconf txq_conf;
	struct rte_eth_rxconf rxq_conf;
	struct rte_eth_dev_info dev_info;

	if (!rte_eth_dev_is_valid_port(port_id))
		rte_exit(EXIT_FAILURE,"Port is not valid\n");

	ret = rte_eth_dev_info_get(port_id, &dev_info);
	if (ret != 0){
		rte_exit(EXIT_FAILURE,
			"Error during getting device (port %u) info: %s\n",
			port_id, strerror(-ret));
	}

	port_conf.txmode.offloads &= dev_info.tx_offload_capa;
	printf(":: initializing port: %d\n", port_id);
	ret = rte_eth_dev_configure(port_id,
				nr_queues, nr_queues, &port_conf);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE,
			":: cannot configure device: err=%d, port=%u\n",
			ret, port_id);
	}

	rxq_conf = dev_info.default_rxconf;
	rxq_conf.offloads = port_conf.rxmode.offloads;
	/* >8 End of ethernet port configured with default settings. */

    printf("create enabled cores\n\tcores: ");
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
    rx_rings = n_lcores * rx_queues_per_lcore;
    tx_rings = n_lcores * tx_queues_per_lcore;

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
            lcore_conf[i].port = port_id;

        }
    }

	// create mbuf pool
	for(i=0; i < n_lcores; i++){
        printf("create mbuf_pool_%d\n",i);
        char name[50];
        sprintf(name,"mbuf_pool_%d",i);
        pktmbuf_pool[i] = rte_mempool_create(
            name,
            NUM_MBUFS,
            MBUF_SIZE,
            MBUF_CACHE_SIZE,
            sizeof(struct rte_pktmbuf_pool_private),
            rte_pktmbuf_pool_init, NULL,
            rte_pktmbuf_init, NULL,
            rte_socket_id(),
            0);
        if (pktmbuf_pool[i] == NULL) {
            rte_exit(EXIT_FAILURE, "cannot init mbuf_pool_%d\n", i);
        }
	}

	ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,"error code: err=%d", ret);
	
	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (i = 0; i < rx_rings; i++) {
		uint32_t core_id = i/tx_queues_per_lcore;
        ret = rte_eth_rx_queue_setup(port_id, i, nb_rxd,
                rte_eth_dev_socket_id(port_id), NULL, pktmbuf_pool[core_id]);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE,
                "rte_eth_rx_queue_setup: err=%d, port=%u\n", ret, port_id);
        }
	}

	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = port_conf.txmode.offloads;

	for (i = 0; i < tx_rings; i++) {
		ret = rte_eth_tx_queue_setup(port_id, i, nb_txd,
				rte_eth_dev_socket_id(port_id),
				&txq_conf);
		if (ret < 0) {
			rte_exit(EXIT_FAILURE,
				":: Tx queue setup failed: err=%d, port=%u\n",
				ret, port_id);
		}
	}
	/* >8 End of Configuring RX and TX queues connected to single port. */

	/* Setting the RX port to promiscuous mode. 8< */
	ret = rte_eth_promiscuous_enable(port_id);
	if (ret != 0)
		rte_exit(EXIT_FAILURE,
			":: promiscuous mode enable failed: err=%s, port=%u\n",
			rte_strerror(-ret), port_id);
	/* >8 End of setting the RX port to promiscuous mode. */

	/* Starting the port. 8< */
	ret = rte_eth_dev_start(port_id);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE,
			"rte_eth_dev_start:err=%d, port=%u\n",
			ret, port_id);
	}
	/* >8 End of starting the port. */

	assert_link_status();

	printf(":: initializing port: %d done\n", port_id);
}
/* >8 End of Port initialization used in flow filtering. */

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\n\nSignal %d received, preparing to exit...\n",
				signum);
		force_quit = true;
	}
}

int
main(int argc, char **argv)
{
	int ret;
	uint16_t nr_ports;

	unsigned lcore_id;
	uint32_t n_lcores = 0;

	/* Initialize EAL. 8< */
	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, ":: invalid EAL arguments\n");
	/* >8 End of Initialization of EAL. */

	force_quit = false;
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	nr_ports = rte_eth_dev_count_avail();
	if (nr_ports == 0)
		rte_exit(EXIT_FAILURE, ":: no Ethernet ports found\n");
	port_id = 0;
	if (nr_ports != 1) {
		printf(":: warn: %d ports detected, but we use only one: port %u\n",
			nr_ports, port_id);
	}

	/* Initializes all the ports using the user defined init_port(). 8< */
	init_port(&n_lcores);
	/* >8 End of Initializing the ports using user defined init_port(). */

    rte_eal_mp_remote_launch((lcore_function_t *)launch_one_lcore, NULL, CALL_MAIN);
    RTE_LCORE_FOREACH_WORKER(lcore_id){
        if (rte_eal_wait_lcore(lcore_id) < 0) {
            ret = -1;
            break;
        } 
    }
	/* Launching main_loop(). 8< */
	ret = packet_send_main_loop();
	/* >8 End of launching main_loop(). */

	/* clean up the EAL */
	rte_eal_cleanup();
	return ret;
}
