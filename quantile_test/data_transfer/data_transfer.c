#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <rte_cycles.h>

#include "data_transfer.h"
#include "xxhash.h"

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
    uint64_t icmp_pkt_num;
    uint64_t flow_capacity;
};

enum quantile_type {
    SIMPLE_QUAN,
    DUMIQE_QUAN,
    QEWA_QUAN,

    QUANTILE_N_TYPE
};

struct output_data{
    enum quantile_type al_type;
    uint64_t flow_num;
    double percentage;
    double lambda;
    double gama;
    double quantile;
    double deviation;
    uint64_t run_time;
};

int isMemoryAllZeros(const void* memory, size_t size) {
    const unsigned char* ptr = (const unsigned char*)memory;

    for (size_t i = 0; i < size; i++) {
        if (ptr[i] != 0) {
            return 0;
        }
    }

    return 1;
}

void flow_count(u_char *user_data, __attribute__((unused)) const struct pcap_pkthdr *pkthdr, const u_char *packet) {
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
        } else if (tuple.protocol == IPPROTO_ICMP) {
            tuple.src_port = 0;
            tuple.dst_port = 0;
            fc_config->icmp_pkt_num ++;
        }
    }

    uint64_t hash_value = XXH64(&tuple, sizeof(struct five_tuple), 123) % (MAX_HASH_FLOW_COUNT / HASH_BUCKET);
    u_int64_t i;
    for (i = 0; i < HASH_BUCKET; i++){
        uint64_t bucket_location = hash_value * HASH_BUCKET + i;

        if (fc_config->flows[bucket_location].count == 0) {
            memcpy(&fc_config->flows[bucket_location].tuple, &tuple, sizeof(struct five_tuple));
            fc_config->flows[bucket_location].count = 1;
            fc_config->flow_num++;
            break;
        }

        if (memcmp(&tuple, &fc_config->flows[bucket_location].tuple, sizeof(struct five_tuple)) == 0){
            fc_config->flows[bucket_location].count++;
            break;
        }
    }
    
    if (i == HASH_BUCKET){
        printf("fail to add flows, hash table is too small");
    }
}

void flow_print(uint64_t flow_num, struct flow_count * flow_table) {
    uint64_t i;
    for (i = 0; i < flow_num; i++) {
        printf("Unique Tuple: %u %u %u %u %u, Count: %d\n",
            flow_table[i].tuple.src_ip,
            flow_table[i].tuple.dst_ip,
            flow_table[i].tuple.src_port,
            flow_table[i].tuple.dst_port,
            flow_table[i].tuple.protocol,
            flow_table[i].count);
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
        .icmp_pkt_num = 0,
        .flow_capacity = MAX_HASH_FLOW_COUNT
    };

    int ret = pcap_loop(handle, 0, flow_count, (u_char *)&flow_count_config);
    if (ret == -1) {
        fprintf(stderr, "Error in pcap_loop: %s\n", pcap_geterr(handle));
        return 1;
    }

    pcap_close(handle);

    printf("total %lu packets, %lu tcp packets, %lu udp packets, %lu icmp packets, %lu flows\n", 
           flow_count_config.packet_num, \
           flow_count_config.tcp_pkt_num, \
           flow_count_config.udp_pkt_num, \
           flow_count_config.icmp_pkt_num, \
           flow_count_config.flow_num);
    return flow_count_config.flow_num;
}


void packet_count(u_char *user_data, __attribute__((unused)) const struct pcap_pkthdr *pkthdr, __attribute__((unused)) const u_char *packet) {
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

/* delete empty flows*/
uint64_t flow_filter(struct flow_count *src_flow_table, struct flow_count *dst_flow_table){
    uint64_t i;
    uint64_t flow_num = 0;
    for(i = 0; i < MAX_HASH_FLOW_COUNT; i++){
        if(src_flow_table[i].count > 0){
            memcpy(&dst_flow_table[flow_num], &src_flow_table[i], sizeof(struct flow_count));
            flow_num++;
            if(flow_num > MAX_FLOW_COUNT){
                printf("outof space\n");
            }
        }
    }

    return flow_num;
}

/* delete flows which is not TCP or UDP or ICMP
   delete empty flows*/
uint64_t tui_flow_filter(struct flow_count *src_flow_table, struct flow_count *dst_flow_table){
    uint64_t i;
    uint64_t flow_num = 0;
    for(i = 0; i < MAX_HASH_FLOW_COUNT; i++){
        if((src_flow_table[i].count > 0) && (src_flow_table[i].tuple.protocol == IPPROTO_TCP || src_flow_table[i].tuple.protocol == IPPROTO_UDP || src_flow_table[i].tuple.protocol == IPPROTO_ICMP)){
            memcpy(&dst_flow_table[flow_num], &src_flow_table[i], sizeof(struct flow_count));
            flow_num++;
            if(flow_num > MAX_FLOW_COUNT){
                printf("outof space\n");
            }
        }
    }

    return flow_num;
}

uint64_t tu_flow_filter(struct flow_count *src_flow_table, struct flow_count *dst_flow_table){
    uint64_t i;
    uint64_t flow_num = 0;
    for(i = 0; i < MAX_HASH_FLOW_COUNT; i++){
        if((src_flow_table[i].count > 0) && (src_flow_table[i].tuple.protocol == IPPROTO_TCP || src_flow_table[i].tuple.protocol == IPPROTO_UDP)){
            memcpy(&dst_flow_table[flow_num], &src_flow_table[i], sizeof(struct flow_count));
            flow_num++;
            if(flow_num > MAX_FLOW_COUNT){
                printf("outof space\n");
            }
        }
    }

    return flow_num;
}

void flow_size_output(const struct flow_count *flow_table, const uint64_t flow_num, char * file_name){
    FILE *fp;
    uint64_t i;

    if (unlikely(access(file_name, 0) != 0)){
        fp = fopen(file_name, "a+");
        if(unlikely(fp == NULL)){
            rte_exit(EXIT_FAILURE, "Cannot open file %s\n", file_name);
        }
        fprintf(fp, "index, size\r\n");
    }else{
        fp = fopen(file_name, "a+");
    }

    for(i = 0; i < flow_num; i++){
        fprintf(fp, "%ld,%d\r\n", i, flow_table[i].count);
    }

}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[]) {
    uint64_t flow_num;
    // struct timeval start, end;
    // uint64_t run_time;
    int total_packet_count = packet_num();
    printf("the pcap file contains %d packets\n", total_packet_count);
    
    struct flow_count *flows = (struct flow_count *) malloc(MAX_HASH_FLOW_COUNT * sizeof(struct flow_count));
    if (!flows) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    memset(flows, 0 , MAX_HASH_FLOW_COUNT * sizeof(struct flow_count));

    //count flows
    flow_num = flow_num_count(flows);
    printf("the number of flows is %lu\n", flow_num);

    uint64_t all_flow_num;
    struct flow_count *all_flows = (struct flow_count *) malloc(MAX_FLOW_COUNT * sizeof(struct flow_count));
    if (!all_flows) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    memset(all_flows, 0 , MAX_FLOW_COUNT * sizeof(struct flow_count));
    all_flow_num = flow_filter(flows, all_flows);
    flow_size_output(all_flows, all_flow_num, "./build/all_flows.csv");
    printf("the number of total flows is %lu\n", all_flow_num);

    uint64_t tcpudp_flow_num;
    struct flow_count *tcpudp_flows = (struct flow_count *) malloc(MAX_FLOW_COUNT * sizeof(struct flow_count));
    if (!tcpudp_flows) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    memset(tcpudp_flows, 0 , MAX_FLOW_COUNT * sizeof(struct flow_count));
    tcpudp_flow_num = tu_flow_filter(flows, tcpudp_flows);
    flow_size_output(tcpudp_flows, tcpudp_flow_num, "./build/tcpudp_flows.csv");
    printf("the number of tcp&udp flows is %lu\n", tcpudp_flow_num);

    uint64_t tui_flow_num;
    struct flow_count *tui_flows = (struct flow_count *) malloc(MAX_FLOW_COUNT * sizeof(struct flow_count));
    if (!tui_flows) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    memset(tui_flows, 0 , MAX_FLOW_COUNT * sizeof(struct flow_count));
    tui_flow_num = tui_flow_filter(flows, tui_flows);
    flow_size_output(tui_flows, tui_flow_num, "./build/tui_flows.csv");
    printf("the number of tcp&udp&icmp flows is %lu\n", tui_flow_num);

    FILE *fp;
    char file_name[] = "./build/info.csv";
    if (unlikely(access(file_name, 0) != 0)){
        fp = fopen(file_name, "a+");
        if(unlikely(fp == NULL)){
            rte_exit(EXIT_FAILURE, "Cannot open file %s\n", file_name);
        }
        fprintf(fp, "pcap,pkt_num,flow_num,\r\n");
    }else{
        fp = fopen(file_name, "a+");
    }


    fprintf(fp, "%s,", PCAP_FILE_NAME);
    fprintf(fp, "%d,", total_packet_count);
    fprintf(fp, "%ld\r\n", flow_num);

    // flow_print(tcpudp_flow_num, tcpudp_flows);

    // free(flows);
    // free(all_flows);
    // free(tcpudp_flows);
    // free(tui_flow_num);

    return 0;
}
