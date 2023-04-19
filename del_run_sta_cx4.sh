file=pkt_send_mul_auto_sta2
remotefile=pkt_rcv_mul_auto_sta
run_path="/home/qyn/software/FastNIC"

rm ./lab_results/log/remote.out
rm ./lab_results/log/cx4.out
rm -f ./lab_results/$file/throughput*
ssh qyn@10.15.198.149 "cd $run_path && rm -f ./lab_results/$remotefile/throughput*"
echo "del former file successfully"
