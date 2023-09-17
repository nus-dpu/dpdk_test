#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <rte_cycles.h>

#include "all_quantile.h"
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
        .flow_capacity = MAX_HASH_FLOW_COUNT
    };

    int ret = pcap_loop(handle, 0, flow_count, (u_char *)&flow_count_config);
    if (ret == -1) {
        fprintf(stderr, "Error in pcap_loop: %s\n", pcap_geterr(handle));
        return 1;
    }

    pcap_close(handle);

    printf("total %lu packets, %lu tcp packets, %lu udp packets, %lu flows\n", 
           flow_count_config.packet_num, \
           flow_count_config.tcp_pkt_num, \
           flow_count_config.udp_pkt_num, \
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

/* delete flows which is not TCP or UDP  
   delete empty flows*/
uint64_t flow_filter(struct flow_count *flow_table, struct flow_count *tcpudp_flow_table){
    uint64_t i;
    uint64_t tcpudp_flow_num = 0;
    for(i = 0; i < MAX_HASH_FLOW_COUNT; i++){
        if((flow_table[i].count > 0) && (flow_table[i].tuple.protocol == IPPROTO_TCP || flow_table[i].tuple.protocol == IPPROTO_UDP)){
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
    uint64_t index = (uint64_t) (percentage * flow_num);
    return flow_list[index].count;
}

double DUMIQE_quantile_cal(struct flow_count *flow_list, uint64_t flow_num, double percentage, double lambda){
    uint64_t i;
    double q_pre = flow_list[0].count;
    for (i = 1; i < flow_num; i++){
        if (flow_list[i].count > q_pre) {
            q_pre = q_pre + lambda * percentage * q_pre;
        } else {
            q_pre = q_pre - lambda * (1-percentage) * q_pre;
        }
    }
    return q_pre;
}

double QEWA_quantile_cal(struct flow_count *flow_list, uint64_t flow_num, double q, double lambda, double gama){
    uint64_t i;
    double Q_pre0 = flow_list[0].count;
    double u_positive = flow_list[0].count+1;
    double u_negative = flow_list[0].count-1;
    double b, a;
    double Q_pre1;

    for (i = 1; i < flow_num; i++){
        a = (q/(u_positive - Q_pre0)) / ((q/(u_positive - Q_pre0)) + ((1 - q)/(Q_pre0 - u_negative)));
        b = ((flow_list[i].count > Q_pre0) ? \
              (lambda * a): \
              (lambda * (1-a)));
        Q_pre1 = (1 - b) * Q_pre0 + b * flow_list[i].count;

        u_positive = ((flow_list[i].count > Q_pre0)? \
                       Q_pre1 - Q_pre0 + (1 - gama) * u_positive + gama * flow_list[i].count:
                       Q_pre1 - Q_pre0 + u_positive);
        u_negative = ((flow_list[i].count > Q_pre0)? \
                       Q_pre1 - Q_pre0 + u_negative:
                       Q_pre1 - Q_pre0 + (1 - gama) * u_negative + gama * flow_list[i].count);
        Q_pre0 = Q_pre1;
    }

    return Q_pre1;
}

int printlog(struct output_data *data){
    FILE *fp;

    if (unlikely(access(OUTPUT_FILE, 0) != 0)){
        fp = fopen(OUTPUT_FILE, "a+");
        if(unlikely(fp == NULL)){
            rte_exit(EXIT_FAILURE, "Cannot open file %s\n", OUTPUT_FILE);
        }
        fprintf(fp, "al_type,flow_num,percentage,lambda,gama,quantile,deviation,run_time\r\n");
    }else{
        fp = fopen(OUTPUT_FILE, "a+");
    }
    
    if (data->al_type < QUANTILE_N_TYPE) {
        switch(data->al_type){
            case SIMPLE_QUAN:
                fprintf(fp, "SIMPLE_QUAN,");
                break;
            case DUMIQE_QUAN:
                fprintf(fp, "DUMIQE_QUAN,");
                break;
            case QEWA_QUAN:
                fprintf(fp, "QEWA_QUAN,");
                break;
            default:
                fprintf(fp, "NONE,");
        }
    }
    fprintf(fp, "%lu,",data->flow_num);
    fprintf(fp, "%lf,",data->percentage);
    if (data->al_type < QUANTILE_N_TYPE) {
        switch(data->al_type){
            case SIMPLE_QUAN:
                fprintf(fp, "-1,");
                break;
            case (DUMIQE_QUAN):
                fprintf(fp, "%lf,",data->lambda);
                break;
            case (QEWA_QUAN):
                fprintf(fp, "%lf,",data->lambda);
                break;
            default:
                fprintf(fp, "NONE,");
        }
    }
    if (data->al_type < QUANTILE_N_TYPE) {
        switch(data->al_type){
            case (SIMPLE_QUAN):
                fprintf(fp, "-1,");
                break;
            case (DUMIQE_QUAN):
                fprintf(fp, "-1,");
                break;
            case (QEWA_QUAN):
                fprintf(fp, "%lf,",data->gama);
                break;
            default:
                fprintf(fp, "NONE,");
        }
    }
    fprintf(fp, "%lf," ,data->quantile);
    fprintf(fp, "%lf," ,data->deviation);
    fprintf(fp, "%lu\r\n", data->run_time);

    return 0;
}

int main(__attribute__((unused)) int argc, __attribute__((unused)) char *argv[]) {
    uint64_t flow_num, tcpudp_flow_num;
    struct timeval start, end;
    uint64_t run_time;
    // int total_packet_count = packet_num();
    // printf("the pcap file contains %d packets\n", total_packet_count);
    
    struct flow_count *flows = (struct flow_count *) malloc(MAX_HASH_FLOW_COUNT * sizeof(struct flow_count));
    if (!flows) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    memset(flows, 0 , MAX_HASH_FLOW_COUNT * sizeof(struct flow_count));

    //count flows
    flow_num = flow_num_count(flows);
    printf("the number of flows is %lu\n", flow_num);

    struct flow_count *tcpudp_flows = (struct flow_count *) malloc(MAX_FLOW_COUNT * sizeof(struct flow_count));
    if (!tcpudp_flows) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }
    memset(tcpudp_flows, 0 , MAX_FLOW_COUNT * sizeof(struct flow_count));

    tcpudp_flow_num = flow_filter(flows, tcpudp_flows);
    printf("the number of tcp&udp flows is %lu\n", tcpudp_flow_num);

    // flow_print(tcpudp_flow_num, tcpudp_flows);
    
    int i,j,k;
    double QUANTILE_PER[] = {0.6,0.65,0.7,0.75,0.8,0.85,0.9,0.95};
    int quan_len = 5;

    for (i = 0; i < quan_len; i++){
        struct flow_count *sq_flows = (struct flow_count *) malloc(tcpudp_flow_num * sizeof(struct flow_count));
        if (!sq_flows) {
            fprintf(stderr, "Memory allocation failed\n");
            return 1;
        }
        memcpy(sq_flows, tcpudp_flows, tcpudp_flow_num * sizeof(struct flow_count));

        gettimeofday(&start, NULL);
        double quantile_true = (double) quantile_cal(sq_flows, tcpudp_flow_num, QUANTILE_PER[i]);
        gettimeofday(&end, NULL);
        run_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
        printf("with method simple_quantile, %lf quantile is %lf, calculate time is %lu us\n", QUANTILE_PER[i]*100, quantile_true, run_time);

        struct output_data data = {
            .al_type = SIMPLE_QUAN,
            .flow_num = flow_num,
            .percentage = QUANTILE_PER[i],
            .quantile = quantile_true,
            .deviation = 0,
            .run_time = run_time
        };
        printlog(&data);

        printf("\nDUMIQE:\n");
        double LAMBDA_DUMIQE[] = {0.01,0.02,0.03,0.04,0.05,0.06,0.07,0.08,0.09,0.1};
        int labda_DUMIQE_len = 10;

        for (j = 0 ; j < labda_DUMIQE_len; j++){
            gettimeofday(&start, NULL);
            double quantile_DUMIQE = DUMIQE_quantile_cal(tcpudp_flows, tcpudp_flow_num, QUANTILE_PER[i],LAMBDA_DUMIQE[j]);
            gettimeofday(&end, NULL);
            run_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);

            printf("    with method DUMIQE_quantile, parameter(lambda:%lf),%lf quantile is %lf, calculate time is %lu us\n", 
                        LAMBDA_DUMIQE[j], QUANTILE_PER[i]*100, quantile_DUMIQE, run_time);

            struct output_data data = {
                .al_type = DUMIQE_QUAN,
                .flow_num = flow_num,
                .percentage = QUANTILE_PER[i],
                .lambda = LAMBDA_DUMIQE[j],
                .quantile = quantile_DUMIQE,
                .deviation = quantile_DUMIQE - quantile_true,
                .run_time = run_time
            };
            printlog(&data);
        }

        printf("\nQEWA:\n");
        double LAMBDA_QEWA[] = {0.01,0.02,0.03,0.04,0.05,0.06,0.07,0.08,0.09,0.1};
        int labda_QEWA_len = 10;
        double ratio[] ={0.01,0.02,0.03,0.04,0.05,0.06,0.07,0.08,0.09,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9};
        int radio_len = 18;
        double gama;

        for (j = 0 ; j < labda_QEWA_len; j++){
            for (k = 0; k < radio_len; k++){
                gama = LAMBDA_QEWA[j] * ratio[k];
                gettimeofday(&start, NULL);
                double quantile_QEWA = QEWA_quantile_cal(tcpudp_flows, tcpudp_flow_num, QUANTILE_PER[i],LAMBDA_QEWA[j], gama);
                gettimeofday(&end, NULL);
                run_time = (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);

                printf("    with method QEWA_quantile, parameter(lambda:%lf,gama:%lf),%lf quantile is %lf, calculate time is %lu us\n", \
                            LAMBDA_QEWA[j], gama, QUANTILE_PER[i]*100, quantile_QEWA, run_time);

                struct output_data data = {
                    .al_type = QEWA_QUAN,
                    .flow_num = flow_num,
                    .percentage = QUANTILE_PER[i],
                    .lambda = LAMBDA_QEWA[j],
                    .gama = gama,
                    .quantile = quantile_QEWA,
                    .deviation = quantile_QEWA - quantile_true,
                    .run_time = run_time
                };
                printlog(&data);
            }
        }
    }

    free(flows);
    free(tcpudp_flows);

    return 0;
}
