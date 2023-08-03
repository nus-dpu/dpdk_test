# parameter
file=pkt_send_mul_auto_sta5
remotefile=pkt_rcv_mul_auto_sta3
run_path="/home/qyn/software/FastNIC/lab_all/lab_1"

ovs_path="/home/ubuntu/software/FastNIC/lab_results/ovs_log"

cd $run_path
rm ./lab_results/log/remote.out
rm ./lab_results/log/cx4.out
# rm ./lab_results/log/remote_bfconfig.out
rm -rf ./lab_results/$file/*
ssh qyn@10.15.198.160 "cd $run_path && rm -rf ./lab_results/$remotefile/*"
rm -rf ./lab_results/ovslog/*
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "cd $ovs_path && rm -f ./*.csv"

echo "del former file successfully"
