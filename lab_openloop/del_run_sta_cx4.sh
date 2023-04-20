file=pkt_send_mul_auto_sta2
remotefile=pkt_rcv_mul_auto_sta
lab=lab_openloop
run_path="/home/qyn/software/FastNIC/$lab"
ovs_path="/home/ubuntu/software/FastNIC/lab_results/ovs_log"

cd $run_path
rm ./lab_results/log/remote.out
rm ./lab_results/log/cx4.out
rm -f ./lab_results/$file/*
ssh qyn@10.15.198.149 "cd $run_path && rm -f ./lab_results/$remotefile/*"
rm -f ./lab_results/ovs_log/*
ssh ubuntu@10.15.198.148 "cd $ovs_path && rm -f ./*.csv"

echo "del former file successfully"