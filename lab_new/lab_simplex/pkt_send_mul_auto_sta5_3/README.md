pkt_send_mul_auto_sta
  based on cz_pkt_send_sampling
  add function:
    1.flow_num can be inputted by shell
    2.results output to csv

pkt_send_mul_auto_sta2
  based on pkt_send_mul_auto_sta
  change function:
    1.change flow pattern (former:random; now:circulation)

pkt_send_mul_auto_sta3
  based on pkt_send_mul_auto_sta
  add function:
    1.add metadata in payload

pkt_send_mul_auto_sta4
  based on pkt_send_mul_auto_sta3
  change function:
    1.read data from pcap and reply it

pkt_send_mul_auto_sta5
  based on pkt_send_mul_auto_sta4
  change function:
    1.generate x*y flows with different "x" src_ip and "y" dst_ip

pkt_send_mul_auto_sta5_1
  based on pkt_send_mul_auto_sta4
  change function:
    1.generate x flows with different "x" src_ip
    2.send x flows with t packets (total x*t packets)

pkt_send_mul_auto_sta5_2
  based on pkt_send_mul_auto_sta4/5_1
  change function:
    1.generate x flows with different "x" src_ip

pkt_send_mul_auto_sta5_3
  based on pkt_send_mul_auto_sta5_2
  change function:
    1.generate x flows with different "x" src_ip, half is small and half is large