#define FLOW_SIZE 4
#define SRC_IP_NUM 8
#define DST_IP_NUM 4
#define FLOW_NUM (SRC_IP_NUM*DST_IP_NUM)
#define PKTS_NUM (FLOW_SIZE*SRC_IP_NUM*DST_IP_NUM)

#define PKT_LEN 64
#define MAX_RECORD_COUNT 30
#define PROGRAM "pkt_send_mul_auto_sta4"
