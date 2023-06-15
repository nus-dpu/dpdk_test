from scapy.all import wrpcap, Ether, IP, TCP
import numpy as np

def pktfile_gen(file, flow_num, pkts_count, pkts_per_write, zipf_a):
    IP_PREFIX = 192<<24
    
    #generate a zipf distribution
    zipf_dist = np.random.zipf(zipf_a, pkts_count)
    zipf_dist_scaled = np.interp(zipf_dist, (zipf_dist.min(), zipf_dist.max()), (0, flow_num-1))
    zipf_dist_int = zipf_dist_scaled.astype(int)    # Generate and store packets incrementally
    packets = []

    for i in range(pkts_count):
        # Create a dummy packet (replace with your actual packet generation logic)
        src_ip = IP_PREFIX + zipf_dist_int[i]
        ethernet = Ether(dst='00:11:22:33:44:55', src='00:AA:BB:CC:DD:EE')
        ip = IP(src=src_ip, dst='192.168.0.2')
        # ip = IP(src='192.0.0.1', dst='192.168.0.2')
        tcp = TCP(sport=1234, dport=80)

        packet = ethernet / ip / tcp
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
    pkt_num = 5000000 # Total number of packets to generate
    zipf_para = 2.0 # the parameter a for zipf
    packets_per_write = 1000  # Number of packets to write in each iteration    print("Hello, world!")

    for flow_num in [100,10000,50000,100000]: # Total number of flows to generate
        filename = "/dev/shm/flow_"+str(flow_num)+".pcap"
        pktfile_gen(filename, flow_num, pkt_num, packets_per_write, zipf_para)
        print(f"{pkt_num} packets written to {filename}.")


if __name__ == "__main__":
    main()

