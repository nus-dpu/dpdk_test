test_time=5
for core_id in {18,18-19}
do
    for flow_num in {100,10000}
    do
        nohup expect remote_run_sta.expect ${test_time}> lab_results/log/remote.out 2>&1 &
        ./start_sta.sh pkt_send_mul_auto_sta enp216s0f0 "192.168.200.2" $core_id $flow_num 64 $test_time #149,bf2tocx4
        sleep(60)
    done
done

# ./start.sh pkt_rcv_multicore enp59s0f0 "192.168.200.2" 0,1 #149,bf2tocx4

# ./start.sh pkt_send_multicore enp216s0 "192.168.201.2" 0,1,2 #149,cx5
# ./start.sh pkt_send_multicore ens1np0 "192.168.200.1" 0,1 #150,cx4tobf2
# ./start.sh pkt_send_multicore ens3np0 "192.168.201.1" 0,1 #150,cx5