from scapy.all import wrpcap, Ether, IP, TCP, Raw
import numpy as np
from collections import Counter
import struct
import socket

def pktfile_gen(file, src_ips, dst_ips):
    STC_IP_PREFIX = 192<<24
    DST_IP_PREFIX = 193<<24

    packets = []

    for i in range(src_ips):
        for j in range(dst_ips):
            # Create a dummy packet (replace with your actual packet generation logic)
            src_ip = socket.inet_ntoa(struct.pack("!I", STC_IP_PREFIX + i))
            dst_ip = socket.inet_ntoa(struct.pack("!I", DST_IP_PREFIX + j))

            ethernet = Ether(dst='00:11:22:33:44:55', src='00:AA:BB:CC:DD:EE')
            ip = IP(src=src_ip, dst=dst_ip)
            tcp = TCP(sport=1234, dport=80)

            flow_size = struct.pack("!I", 1)
            pkt_seq = struct.pack("!I", 1)
            timestamp =  struct.pack("!Q", 20)
            payload = Raw(load = flow_size + pkt_seq + timestamp)

            packet = ethernet / ip / tcp / payload
            packets.append(packet)

    if packets:
        wrpcap(file, packets)



def main():
    # Parameters for packet generation and storage
    # pkt_num = 10000000 # Total number of packets to generate
    src_ips = 2
    dst_ips = 2
    flow_num = src_ips * dst_ips

    filename = "/dev/shm/flow_"+str(flow_num)+".pcap"
    pktfile_gen(filename, src_ips, dst_ips)
    print(f"{flow_num} packets ({flow_num}flows in uniform distribution) written to {filename}.")

    # for flow_num in [100, 1000, 5000, 10000, 20000, 50000, 70000, 100000, 200000, 500000, 700000, 1000000, 2000000, 5000000, 7000000, 10000000]: # Total number of flows to generate
    #     filename = "/dev/shm/flow_"+str(flow_num)+".pcap"
    #     pktfile_gen(filename, flow_num, pkt_num)
    #     print(f"{pkt_num} packets ({flow_num}flows in uniform distribution) written to {filename}.")


if __name__ == "__main__":
    main()

