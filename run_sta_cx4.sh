test_time_rcv=120
test_time_send=100
file=pkt_send_mul_auto_sta2
remotefile=pkt_rcv_mul_auto_sta
line="bf2"
# line="cx5"

flow_size=1000000000

user="qyn"
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
for core_id in {"0-31","0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30","0,2,4,6"}
do
    # for flow_num in {100,1000,10000,30000,50000,70000,90000,100000}
    for flow_num in {1000,100000}
    do
        nohup expect remote_run_sta_cx4.expect $test_time_rcv $run_path $user $password $remotefile $rcv_nic >> ./lab_results/log/remote.out 2>&1 &
        sleep 8s
        echo ./start_sta.sh $file $line 150 $core_id $flow_num 64 $flow_size $test_time_send $run_path
        ./start_sta.sh $file $line 150 $core_id $flow_num 64 $flow_size $test_time_send $run_path #149,bf2tocx4
         # sudo ./build/pkt_send_mul_auto_sta3 -l 0,2 -a 0000:5e:00.0 -- --srcmac b8:83:03:82:a2:10 --dstmac 0c:42:a1:d8:10:84
        sleep 30s
    done
done

# ./start.sh pkt_rcv_multicore enp59s0f0 "192.168.200.2" 0,1 #149,bf2tocx4

# ./start.sh pkt_send_multicore enp216s0 "192.168.201.2" 0,1,2 #149,cx5
# ./start.sh pkt_send_multicore ens1np0 "192.168.200.1" 0,1 #150,cx4tobf2
# ./start.sh pkt_send_multicore ens3np0 "192.168.201.1" 0,1 #150,cx5
