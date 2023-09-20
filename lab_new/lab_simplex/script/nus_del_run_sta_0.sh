# parameter
sendfile=pkt_send_mul_auto_sta5_3
recvfile=pkt_rcv_mul_auto_sta3
lab=lab_simplex
bf=ubuntu@192.168.100.2
remote_server=qyn@nsl-node12.d2.comp.nus.edu.sg

run_path="/local/qyn/software/FastNIC/lab_new/$lab"
ovs_path="/home/ubuntu/software/FastNIC/lab_results/ovs_log"

rm $run_path/lab_results/log/recv.out
rm $run_path/lab_results/log/send.out
rm -rf $run_path/lab_results/$sendfile/*
rm -rf $run_path/lab_results/$recvfile/*
rm -rf $run_path/lab_results/ovslog/*
ssh $bf "rm -f $ovs_path/*.csv"

echo "del former file successfully"
