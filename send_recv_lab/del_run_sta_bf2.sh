#the version is to old, if want to use, please mmodify according to cx4 file

file=pkt_send_mul_auto_sta2
remotefile=pkt_rcv_mul_auto_sta
run_path="/home/qyn/software/FastNIC/send_recv_lab"

cd $run_path
rm ./lab_results/log/remote.out
rm ./lab_results/log/bf.out
rm -f ./lab_results/$file/throughput*
ssh qyn@10.15.198.150 "cd $run_path && rm -f ./lab_results/$remotefile/throughput*"
echo "del former file successfully"
