#define FLOW_SIZE 50000000
#define SRC_IP_NUM 2
#define DST_IP_NUM 1
#define FLOW_NUM (SRC_IP_NUM*DST_IP_NUM)
#define PKTS_NUM (FLOW_SIZE*SRC_IP_NUM*DST_IP_NUM)

#define PKT_LEN 64
#define MAX_RECORD_COUNT 10
#define PROGRAM "pkt_send_mul_auto_sta5"
