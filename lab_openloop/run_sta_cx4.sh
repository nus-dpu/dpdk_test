test_time_rcv=120
test_time_send=100
lab=lab_openloop
file=pkt_send_mul_auto_sta3
remotefile=pkt_rcv_mul_auto_sta
# src_nic=ens3np0 #cx5
src_nic=ens1np0 #cx4
# src_nic=enp175s0 #cx5
rcv_nic=enp216s0f0 #bf2

flow_size=100000

user="qyn"
if [[ ${user} == "cz" ]]
then
    run_path="/home/cz/3_20/FastNIC"
    password="123456"
elif [[ ${user} == "qyn" ]]
then
    run_path="/home/qyn/software/FastNIC/$lab"
    password="nesc77qq"
fi

i=0
# for core_id in {18,18-19,18-21,18-23,18-25,18-27,18-29,18-31,18-33,18-35}
# for core_id in {"0-31","0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30","0,2,4,6"}
for core_id in {"0","0,2"}
do
    # for flow_num in {100,1000,10000,30000,50000,70000,90000,100000}
    for flow_num in {1000,100000}
    do
        nohup expect remote_run_sta_cx4.expect $test_time_rcv $run_path $user $password $remotefile $rcv_nic >> ./lab_results/log/remote.out 2>&1 &
        sleep 8s
        ./start_sta.sh $file $src_nic "192.168.200.1" $core_id $flow_num 64 $flow_size $test_time_send $run_path 150 #149,bf2tocx4
        # ./start_sta.sh pkt_send_mul_auto_sta2/ enp216s0f0 "192.168.200.2" 18-20 100 64 20 /home/qyn/software/FastNIC 149
        sleep 30s

        ((i++))
        ovsfile="/home/ubuntu/software/FastNIC/lab_results/ovs_log"
        rename "s/.csv/_$i.csv/" $run_path/lab_results/$file/*.csv
        ssh qyn@10.15.198.149 "cd $run_path && rename 's/.csv/_$i.csv/' ./lab_results/$remotefile/*.csv"
        scp ubuntu@10.15.198.148:$ovsfile/*.csv $run_path/lab_results/ovs_log
        ssh qyn@10.15.198.148 "cd $ovsfile && rm -f ./*.csv"
        rename "s/.csv/_$i.csv/" $run_path/lab_results/ovs_log/*.csv
    done
done

# ./start.sh pkt_rcv_multicore enp59s0f0 "192.168.200.2" 0,1 #149,bf2tocx4

# ./start.sh pkt_send_multicore enp216s0 "192.168.201.2" 0,1,2 #149,cx5
# ./start.sh pkt_send_multicore ens1np0 "192.168.200.1" 0,1 #150,cx4tobf2
# ./start.sh pkt_send_multicore ens3np0 "192.168.201.1" 0,1 #150,cx5