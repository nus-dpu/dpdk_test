file=pkt_rcvcir_mul_auto_sta
remotefile=pkt_sendcir_mul_auto_sta2

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

if [[ ! -d "./lab_results/ovslog" ]]
then
    mkdir -p ./lab_results/ovslog
fi

if [[ ! -d "./lab_results/log" ]]
then
    mkdir -p ./lab_results/log
fi

core_id="0"
flow_num=-1
pkt_len=-1
test_time_rcv=50
test_time_send=30
srcip_num=-1
dstip_num=-1

flow_size_list=(1 2 3 4 5 6 7 8 9 10 20 30 40 50 60 70 80 90 100)


i=0
ssh ubuntu@10.15.198.148 "sudo ~/bin/ovs-vsctl --no-wait set Open_vSwitch . other_config:hw-offload=true"
ssh ubuntu@10.15.198.148 "sudo ~/bin/ovs-ofctl del-flows ovsdpdk"
ssh ubuntu@10.15.198.148 "sudo ~/bin/ovs-ctl restart --system-id=random"
ssh ubuntu@10.15.198.148 "cd ~/software/FastNIC/lab_all/lab_1.1 && ./myovs_rule_install.sh "
for test in {0..2}
do
    flow_size=${flow_size_list[$test]}

    # echo "expect remote_bf2_config.expect $off_thre"
    # expect remote_bf2_config.expect $off_thre

    send_run_para="flow_num $flow_num pkt_len $pkt_len flow_size $flow_size test_time $test_time_send srcip_num $srcip_num dstip_num $dstip_num"
    rcv_run_para="flow_num $flow_num pkt_len 64 flow_size $flow_size test_time $test_time_rcv srcip_num $srcip_num dstip_num $dstip_num"

    echo "nohup expect remote_run_sta_cx4.expect $run_path $user $password $remotefile $line \"$rcv_run_para\" >> ./lab_results/log/remote.out 2>&1 &"
    nohup expect remote_run_sta_cx4.expect $run_path $user $password $remotefile $line "$rcv_run_para" >> ./lab_results/log/remote.out 2>&1 &
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


ssh ubuntu@10.15.198.148 "sudo ~/bin/ovs-vsctl --no-wait set Open_vSwitch . other_config:hw-offload=false"
ssh ubuntu@10.15.198.148 "sudo ~/bin/ovs-ofctl del-flows ovsdpdk"
ssh ubuntu@10.15.198.148 "sudo ~/bin/ovs-ctl restart --system-id=random"
ssh ubuntu@10.15.198.148 "cd ~/software/FastNIC/lab_all/lab_1.1 && ./myovs_rule_install.sh "
for test in {0..18}
do
    flow_size=${flow_size_list[$test]}

    # echo "expect remote_bf2_config.expect $off_thre"
    # expect remote_bf2_config.expect $off_thre

    send_run_para="flow_num $flow_num pkt_len $pkt_len flow_size $flow_size test_time $test_time_send srcip_num $srcip_num dstip_num $dstip_num"
    rcv_run_para="flow_num $flow_num pkt_len 64 flow_size $flow_size test_time $test_time_rcv srcip_num $srcip_num dstip_num $dstip_num"

    echo "nohup expect remote_run_sta_cx4.expect $run_path $user $password $remotefile $line \"$rcv_run_para\" >> ./lab_results/log/remote.out 2>&1 &"
    nohup expect remote_run_sta_cx4.expect $run_path $user $password $remotefile $line "$rcv_run_para" >> ./lab_results/log/remote.out 2>&1 &
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
