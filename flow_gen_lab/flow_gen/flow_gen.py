from scapy.all import *
import numpy as np
from collections import Counter
import struct

def pktfile_gen(file, pkts_count):    
    pkt_freq_cnt = 0

    packets = []

    for i in range(pkts_count):
        # Create a dummy packet (replace with your actual packet generation logic)
        ethernet = Ether(dst='00:11:22:33:44:55', src='00:AA:BB:CC:DD:EE')
        ip = IP(src='192.0.0.1', dst='192.168.0.2')
        tcp = TCP(sport=1234, dport=80)

        flow_size = struct.pack("!I", pkts_count)
        pkt_freq_cnt = pkt_freq_cnt + 1
        pkt_seq = struct.pack("!I", pkt_freq_cnt)
        timestamp =  struct.pack("!Q", 20)
        payload = Raw(load = flow_size + pkt_seq + timestamp)
        
        packet = ethernet / ip / tcp / payload
        packets.append(packet)

    if packets:
        wrpcap(file, packets)


def main():
    # Parameters for packet generation and storage
    pkt_num = 10 # Total number of packets to generate

    filename = "/dev/shm/flow_1.pcap"
    pktfile_gen(filename, pkt_num)
    print(f"{pkt_num} packets written to {filename}.")

if __name__ == "__main__":
    main()

