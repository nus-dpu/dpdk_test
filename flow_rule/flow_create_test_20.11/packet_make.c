#include <arpa/inet.h>

struct rte_mbuf *make_testpkt(struct rte_mempool *pktmbuf_pool);

static uint32_t reversebytes_uint32t(uint32_t value){
    return (value & 0x000000FFU) << 24 | (value & 0x0000FF00U) << 8 | 
        (value & 0x00FF0000U) >> 8 | (value & 0xFF000000U) >> 24; 
}

static void
fill_ethernet_header(struct rte_ether_hdr *eth_hdr, const struct rte_ether_addr *src_mac, const struct rte_ether_addr *dst_mac) {
	struct rte_ether_addr s_addr = *src_mac; 
	struct rte_ether_addr d_addr = *dst_mac;
	eth_hdr->s_addr =s_addr;
	eth_hdr->d_addr =d_addr;
	eth_hdr->ether_type = rte_cpu_to_be_16(0x0800);
}

static void
fill_ipv4_header(struct rte_ipv4_hdr *ipv4_hdr) {
	ipv4_hdr->version_ihl = (4 << 4) + 5; // ipv4, length 5 (*4)
	ipv4_hdr->type_of_service = 0; // No Diffserv
	ipv4_hdr->total_length = rte_cpu_to_be_16(PKT_LEN); // tcp 20
	ipv4_hdr->packet_id = rte_cpu_to_be_16(5462); // set random
	ipv4_hdr->fragment_offset = rte_cpu_to_be_16(0);
	ipv4_hdr->time_to_live = 64;
	ipv4_hdr->next_proto_id = 17; // udp
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
    udp_hdr->dst_port = rte_cpu_to_be_16(0x1503);
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

struct rte_mbuf *make_testpkt(struct rte_mempool *pktmbuf_pool)
{
    struct rte_mbuf *mp = rte_pktmbuf_alloc(pktmbuf_pool);

    struct rte_ether_addr src_mac = {{0xB8,0x83,0x03,0x82,0xA2,0x10}};//cx4
    struct rte_ether_addr dst_mac = {{0x0C,0x42,0xA1,0xD8,0x10,0x84}};//bf2

    uint16_t pkt_len = PKT_LEN+sizeof(struct rte_ether_hdr);
    char *buf = rte_pktmbuf_append(mp, pkt_len);
    if (unlikely(buf == NULL)) {
        rte_pktmbuf_free(mp);
        return NULL;
    }

    mp->data_len = pkt_len;
    mp->pkt_len = pkt_len;
    uint16_t curr_ofs = 0;

    struct rte_ether_hdr *ether_h = rte_pktmbuf_mtod_offset(mp, struct rte_ether_hdr *, curr_ofs);
	fill_ethernet_header(ether_h, &src_mac, &dst_mac);
    curr_ofs += sizeof(struct rte_ether_hdr);

    struct rte_ipv4_hdr *ipv4_h = rte_pktmbuf_mtod_offset(mp, struct rte_ipv4_hdr *, curr_ofs);
	fill_ipv4_header(ipv4_h);
    curr_ofs += sizeof(struct rte_ipv4_hdr);

    // struct rte_tcp_hdr *tcp_h = rte_pktmbuf_mtod_offset(mp, struct rte_tcp_hdr *, curr_ofs);
	// fill_tcp_header(tcp_h, ipv4_h);
    // curr_ofs += sizeof(struct rte_tcp_hdr);

    struct rte_udp_hdr *udp_h = rte_pktmbuf_mtod_offset(mp, struct rte_udp_hdr *, curr_ofs);
	fill_udp_header(udp_h, ipv4_h);
    curr_ofs += sizeof(struct rte_udp_hdr);
    
    // struct payload * payload_data= rte_pktmbuf_mtod_offset(mp, struct payload *, curr_ofs);
    // payload_data->data[0] = 1;

    return mp;
}