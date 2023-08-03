ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "sed -i 's/#define OFFLOAD_THRE.*$/#define OFFLOAD_THRE ${ofld_pkts}/' /home/ubuntu/software/ovs_all/myovs/lib/fastnic_offload.h"
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "sudo ~/bin/ovs-ctl stop"
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "cd /home/ubuntu/software/ovs_all/myovs && make -j8"
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "cd /home/ubuntu/software/ovs_all/myovs && make install"
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "sudo ~/bin/ovs-ctl start --system-id=random"
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "/home/ubuntu/software/auto_run/myovs_rules_install_file_quick.sh"



