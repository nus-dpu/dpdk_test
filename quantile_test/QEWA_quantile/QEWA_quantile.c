#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <rte_cycles.h>

#include "simple_quantile.h"

struct five_tuple {
    u_int32_t src_ip;
    u_int32_t dst_ip;
    u_int16_t src_port;
    u_int16_t dst_port;
    u_int8_t protocol;
};

struct flow_count {
    struct five_tuple tuple;
    int count;
};

struct flow_count_configure{
    struct flow_count *flows;
    uint64_t flow_num;
    uint64_t packet_num;
    uint64_t tcp_pkt_num;
    uint64_t udp_pkt_num;
    uint64_t flow_capacity;
};

void flow_count(u_char *user_data, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    struct flow_count_configure *fc_config = (struct flow_count_configure *)user_data; 
    struct five_tuple tuple;

    struct ip *ip_header = (struct ip *)(packet + 14); // Assuming Ethernet header
    fc_config->packet_num ++;

    if (ip_header->ip_v == 4) { // IPv4 only
        tuple.src_ip = ip_header->ip_src.s_addr;
        tuple.dst_ip = ip_header->ip_dst.s_addr;
        tuple.protocol = ip_header->ip_p;
        
        if (tuple.protocol == IPPROTO_TCP) {
            struct tcphdr *tcp_header = (struct tcphdr *)(packet + 14 + ip_header->ip_hl * 4);
            tuple.src_port = ntohs(tcp_header->th_sport);
            tuple.dst_port = ntohs(tcp_header->th_dport);
            fc_config->tcp_pkt_num ++;
        } else if (tuple.protocol == IPPROTO_UDP) {
            struct udphdr *udp_header = (struct udphdr *)(packet + 14 + ip_header->ip_hl * 4);
            tuple.src_port = ntohs(udp_header->uh_sport);
            tuple.dst_port = ntohs(udp_header->uh_dport);
            fc_config->udp_pkt_num ++;
        }
    }

    int i;
    for (i = 0; i < fc_config->flow_num; i++) {
        if (memcmp(&tuple, &fc_config->flows[i].tuple, sizeof(struct five_tuple)) == 0) {
            fc_config->flows[i].count++;
            break;
        }
    }

    // 不存在则添加新独立五元组
    if (i == fc_config->flow_num) {
        if (fc_config->flow_num >= fc_config->flow_capacity) {
            printf("%lu flow memory is too less",  fc_config->flow_capacity);
            fc_config->flow_capacity *= 2;
            fc_config->flows = realloc(fc_config->flows, fc_config->flow_capacity * sizeof(struct flow_count));
            if (!fc_config->flows) {
                fprintf(stderr, "Memory reallocation failed\n");
                exit(1);
            }
        }

        memcpy(&fc_config->flows[fc_config->flow_num].tuple, &tuple, sizeof(struct five_tuple));
        fc_config->flows[fc_config->flow_num].count = 1;
        fc_config->flow_num++;
    }
}

void flow_print(struct flow_count_configure * fc_config) {
    printf("total %lu packets, %lu tcp packets, %lu udp packets, %lu flows\n", 
           fc_config->packet_num, fc_config->tcp_pkt_num, fc_config->udp_pkt_num, fc_config->flow_num);
    for (int i = 0; i < fc_config->flow_num; i++) {
        printf("Unique Tuple: %u %u %u %u %u, Count: %d\n",
            fc_config->flows[i].tuple.src_ip,
            fc_config->flows[i].tuple.dst_ip,
            fc_config->flows[i].tuple.src_port,
            fc_config->flows[i].tuple.dst_port,
            fc_config->flows[i].tuple.protocol,
            fc_config->flows[i].count);
    }
}

uint64_t flow_num_count(struct flow_count *flow_table){
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_offline(PCAP_FILE_NAME, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Error opening pcap file: %s\n", errbuf);
        return 1;
    }

    // start packet processing
    struct flow_count_configure flow_count_config = {
        .flows = flow_table,
        .flow_num = 0,
        .packet_num = 0,
        .tcp_pkt_num = 0,
        .udp_pkt_num = 0,
        .flow_capacity = MAX_FLOW_COUNT
    };

    int ret = pcap_loop(handle, 0, flow_count, (u_char *)&flow_count_config);
    if (ret == -1) {
        fprintf(stderr, "Error in pcap_loop: %s\n", pcap_geterr(handle));
        return 1;
    }

    pcap_close(handle);

    flow_print(&flow_count_config);

    free(flow_count_config.flows);
    return flow_count_config.flow_num;
}


void packet_count(u_char *user_data, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    int *packet_count_ptr = (int *)user_data;  // 将用户自定义参数转换为整型指针
    (*packet_count_ptr)++;  // 递增数据包数量
}

int packet_num(void){
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_t *handle = pcap_open_offline(PCAP_FILE_NAME, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Error opening pcap file: %s\n", errbuf);
        return 1;
    }

    // start packet processing
    int total_packet_count = 0;
    int ret = pcap_loop(handle, 0, packet_count, (u_char *)&total_packet_count);
    if (ret == -1) {
        fprintf(stderr, "Error in pcap_loop: %s\n", pcap_geterr(handle));
        return 1;
    }

    pcap_close(handle);

    return total_packet_count;
}

/* delete flows which is not TCP and UDP */
uint64_t flow_filter(uint64_t flow_num, struct flow_count *flow_table, struct flow_count *tcpudp_flow_table){
    int i;
    uint64_t tcpudp_flow_num = 0;
    for(i = 0; i < flow_num; i++){
        if(flow_table[i].tuple.protocol == IPPROTO_TCP || flow_table[i].tuple.protocol == IPPROTO_UDP){
            memcpy(&tcpudp_flow_table[tcpudp_flow_num], &flow_table[i], sizeof(struct flow_count));
            tcpudp_flow_num++;
        }
    }

    return tcpudp_flow_num;
}

int compare_flow_count(const void *a, const void *b) {
    return ((struct flow_count *)a)->count - ((struct flow_count *)b)->count;
}

int quantile_cal(struct flow_count *flow_list, uint64_t flow_num, double percentage){
    qsort(flow_list, flow_num, sizeof(struct flow_count), compare_flow_count);
    int index = (int) (percentage * flow_num);
    return flow_list[index].count;
}

int main(int argc, char *argv[]) {
    uint64_t flow_num;
    uint64_t start;
    // int total_packet_count = packet_num();
    // printf("the pcap file contains %d packets\n", total_packet_count);
    
    struct flow_count *flows = (struct flow_count *) malloc(MAX_FLOW_COUNT * sizeof(struct flow_count));
    if (!flows) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    //count flows
    flow_num = flow_num_count(flows);

    struct flow_count *tcpudp_flows = (struct flow_count *) malloc(MAX_FLOW_COUNT * sizeof(struct flow_count));
    if (!tcpudp_flows) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    flow_filter(flow_num, flows, tcpudp_flows);
    start = rte_rdtsc();
    int quantile = quantile_cal(tcpudp_flows, flow_num, QUANTILE_PER);
    uint64_t cpu_cycles = rte_rdtsc() - start;
    double run_time = cpu_cycles / rte_get_timer_hz();
    printf("%lf quantile is %d, calculate time is %lfs", QUANTILE_PER*100, quantile, run_time);

    return 0;
}
