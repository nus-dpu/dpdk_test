test_time_rcv=1220
test_time_send=1200
file=pkt_send_mul_auto_sta2
remotefile=pkt_rcv_mul_auto_sta
src_nic=enp175s0 #cx5
 #src_nic=enp216s0f0 #bf2
rcv_nic=ens3np0 #cx5
#rcv_nic=ens1np0 #cx4
user=$1
if [[ ${user} == "cz" ]]
then
    run_path="/home/cz/3_20/FastNIC"
    password="123456"
elif [[ ${user} == "qyn" ]]
then
    run_path="/home/qyn/software/FastNIC"
    password="nesc77qq"
fi

# for core_id in {18,18-19,18-21,18-23,18-25,18-27,18-29,18-31,18-33,18-35}
for core_id in {18-29,18-21}
do
    # for flow_num in {100,1000,10000,30000,50000,70000,90000,100000}
    for flow_num in {1000,100000}
    do
        nohup expect remote_run_sta.expect $test_time_rcv $run_path $user $password $remotefile $rcv_nic>> ./lab_results/log/remote.out 2>&1 &
        sleep 8s
        ./start_sta.sh $file $src_nic "192.168.200.2" $core_id $flow_num 64 $test_time_send $run_path 149 #149,bf2tocx4
        # ./start_sta.sh pkt_send_mul_auto_sta2/ enp216s0f0 "192.168.200.2" 18-20 100 64 20 /home/qyn/software/FastNIC 149
        sleep 30s
    done
done

# ./start.sh pkt_rcv_multicore enp59s0f0 "192.168.200.2" 0,1 #149,bf2tocx4

# ./start.sh pkt_send_multicore enp216s0 "192.168.201.2" 0,1,2 #149,cx5
# ./start.sh pkt_send_multicore ens1np0 "192.168.200.1" 0,1 #150,cx4tobf2
# ./start.sh pkt_send_multicore ens3np0 "192.168.201.1" 0,1 #150,cx5
