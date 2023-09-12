from scapy.all import wrpcap, Ether, IP, TCP, Raw
import numpy as np
from collections import Counter
import struct
import socket

def pktfile_gen(file, flow_num, pkts_count, pkts_per_write, zipf_a):
    IP_PREFIX = 192<<24
    
    #generate a zipf distribution
    zipf_dist = np.random.zipf(zipf_a, pkts_count)
    zipf_dist_scaled = np.interp(zipf_dist, (zipf_dist.min(), zipf_dist.max()), (0, flow_num-1))
    zipf_dist_int = zipf_dist_scaled.astype(int)    
    frequency = Counter(zipf_dist_int)
    pkt_freq_cnt = np.zeros(flow_num, dtype=int)

    packets = []

    for i in range(pkts_count):
        # Create a dummy packet (replace with your actual packet generation logic)
        src_ip = socket.inet_ntoa(struct.pack("!I", IP_PREFIX + zipf_dist_int[i]))
        ethernet = Ether(dst='00:11:22:33:44:55', src='00:AA:BB:CC:DD:EE')
        ip = IP(src=src_ip, dst='192.168.0.2')
        # ip = IP(src='192.0.0.1', dst='192.168.0.2')
        tcp = TCP(sport=1234, dport=80)

        flow_size = struct.pack("!I", frequency[zipf_dist_int[i]])
        pkt_freq_cnt[zipf_dist_int[i]] = pkt_freq_cnt[zipf_dist_int[i]] + 1
        pkt_seq = struct.pack("!I", pkt_freq_cnt[zipf_dist_int[i]])
        timestamp =  struct.pack("!Q", 20)
        payload = Raw(load = flow_size + pkt_seq + timestamp)

        packet = ethernet / ip / tcp / payload
        packets.append(packet)

        # Write packets to file in batches
        # if len(packets) >= pkts_per_write:
        #     wrpcap(file, packets, append=True)
        #     packets = []

    # Write any remaining packets
    # if packets:
    #     wrpcap(file, packets, append=True)
    if packets:
        wrpcap(file, packets)



def main():
    # Parameters for packet generation and storage
    pkt_num = 1000000 # Total number of packets to generate
    packets_per_write = 500  # Number of packets to write in each iteration    print("Hello, world!")
    zipf_para = 2.0
    flow_num = 100

    # for flow_num in [100, 10000, 50000, 100000]: # Total number of flows to generate
    # for zipf_para in [1.1, 1.2, 1.3, 1.5, 1.7, 2.0]:
    #     for flow_num in [100, 1000, 10000, 20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000, 100000]:
    filename = "/dev/shm/flow_"+str(flow_num)+"_zipf_"+str(zipf_para)+".pcap"
    pktfile_gen(filename, flow_num, pkt_num, packets_per_write, zipf_para)
    print(f"{pkt_num} packets ({flow_num}flows in {zipf_para} zipf distribution) written to {filename}.")


if __name__ == "__main__":
    main()

