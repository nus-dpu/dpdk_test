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
  based on pkt_send_mul_auto_sta2
  add function:
    1.add metadata in payload

pkt_loopsend_mul_sta
    based on pkt_send_mul_auto_sta3
    1.the script send and receive packets in the meantime
    2.print rx and tx count
    3.send pattern is send FLOW_NUM flows whose size is all FLOW_SIZE
