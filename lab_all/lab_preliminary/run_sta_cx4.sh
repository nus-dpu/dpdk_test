test_time_rcv=30
test_time_send=10
# test_time_rcv=320
# test_time_send=300
file=pkt_send_mul_auto_sta4
remotefile=pkt_rcv_mul_auto_sta3
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
    run_path="/home/qyn/software/FastNIC/lab_all/lab_preliminary"
    password="nesc77qq"
fi

i=0

core_id="0"
for off_thre in {1,2,5,7,10,30}
# for core_id in {18,18-19,18-21,18-23,18-25,18-27,18-29,18-31,18-33,18-35}
# for core_id in {"0-31","0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30","0,2,4,6,8,10,12,14","0,2,4,6","0,2"}
# for core_id in {"0","1"}
do
    echo "expect remote_bf2_config.expect $off_thre"
    expect remote_bf2_config.expect $off_thre
    # for flow_num in {100,10000,50000,100000}
    for flow_num in {100,100000}
    do
        nohup expect remote_run_sta_cx4.expect $test_time_rcv $run_path $user $password $remotefile $line >> ./lab_results/log/remote.out 2>&1 &
        echo "nohup expect remote_run_sta_cx4.expect $test_time_rcv $run_path $user $password $remotefile $line >> ./lab_results/log/remote.out 2>&1 &"
        sleep 8s
        echo "./start_sta.sh $file $line 150 $core_id $flow_num 64 $flow_size $test_time_send $run_path"
        ./start_sta.sh $file $line 150 $core_id $flow_num 64 $flow_size $test_time_send $run_path #149,bf2tocx4
        # ./start_sta.sh pkt_send_mul_auto_sta3 bf2 150 0 1000 64 100000 10 /home/qyn/software/FastNIC/lab_openloop
        # sudo ./build/pkt_send_mul_auto_sta3 -l 0,2 -a 0000:5e:00.0 -- --srcmac b8:83:03:82:a2:10 --dstmac 0c:42:a1:d8:10:84
        sleep 30s

        ((i++))

        mkdir ./lab_results/${file}/send_$i
        mv ./lab_results/${file}/*.csv ./lab_results/${file}/send_$i/
        echo -e "off_thre\r\n${off_thre}" > ./lab_results/${file}/send_$i/para.csv
        
        ssh qyn@10.15.198.149 "cd $run_path && mkdir ./lab_results/${remotefile}/rcv_$i"
        ssh qyn@10.15.198.149 "cd $run_path/lab_results/${remotefile}/ && mv *csv rcv_$i/"
       
        ovsfile_path="/home/ubuntu/software/FastNIC/lab_results/ovs_log"
        mkdir ./lab_results/ovslog/log_$i
        scp ubuntu@10.15.198.148:$ovsfile_path/*.csv $run_path/lab_results/ovslog/log_$i
        ssh ubuntu@10.15.198.148 "cd $ovsfile_path && rm -f ./*.csv"
    done
done

# ./start.sh pkt_rcv_multicore enp59s0f0 "192.168.200.2" 0,1 #149,bf2tocx4

# ./start.sh pkt_send_multicore enp216s0 "192.168.201.2" 0,1,2 #149,cx5
# ./start.sh pkt_send_multicore ens1np0 "192.168.200.1" 0,1 #150,cx4tobf2
# ./start.sh pkt_send_multicore ens3np0 "192.168.201.1" 0,1 #150,cx5
