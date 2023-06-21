file=pkt_send_mul_auto_sta5
remotefile=pkt_rcv_mul_auto_sta3

line="bf2"
# line="cx5"

user="qyn"
if [[ ${user} == "cz" ]]
then
    run_path="/home/cz/3_20/FastNIC"
    password="123456"
elif [[ ${user} == "qyn" ]]
then
    run_path="/home/qyn/software/FastNIC/lab_all/lab_1"
    password="nesc77qq"
fi


core_id="0"
flow_num=-1
pkt_len=-1
test_time_rcv=50
test_time_send=30

# flow_size_list=(1 2 5 10 100 1000 2000 5000 10000)
# srcip_num_list=(10000 10000 1000 1000 1000 1000 1000 1000 1000)
# dstip_num_list=(10000 5000 20000 10000 1000 100 50 20 10)

flow_size_list=(1 2)
srcip_num_list=(10000 10000)
dstip_num_list=(10000 5000)

i=0
for test in {0..1}
# for core_id in {18,18-19,18-21,18-23,18-25,18-27,18-29,18-31,18-33,18-35}
# for core_id in {"0-31","0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30","0,2,4,6,8,10,12,14","0,2,4,6","0,2"}
# for core_id in {"0","1"}
do
    flow_size=${flow_size_list[$test]}
    srcip_num=${srcip_num_list[$test]}
    dstip_num=${dstip_num_list[$test]}

    # echo "expect remote_bf2_config.expect $off_thre"
    # expect remote_bf2_config.expect $off_thre

    send_run_para="flow_num $flow_num pkt_len $pkt_len flow_size $flow_size test_time $test_time_send srcip_num $srcip_num dstip_num $dstip_num"
    rcv_run_para="flow_num $flow_num pkt_len 64 flow_size $flow_size test_time $test_time_rcv srcip_num $srcip_num dstip_num $dstip_num"

    echo nohup expect remote_run_sta_cx4.expect $run_path $user $password $remotefile $line $rcv_run_para >> ./lab_results/log/remote.out 2>&1 &
    nohup expect remote_run_sta_cx4.expect $run_path $user $password $remotefile $line $rcv_run_para >> ./lab_results/log/remote.out 2>&1 &
    sleep 8s

    echo ./start_sta.sh $file $line 150 $core_id $run_path "$send_run_para"
    ./start_sta.sh $file $line 150 $core_id $run_path "$send_run_para" #149,bf2tocx4
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

