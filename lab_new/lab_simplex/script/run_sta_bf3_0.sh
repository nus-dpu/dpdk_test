# node2 send packets to node1
# 均匀分布，不同发包侧core，不同flow number，不同pkt size，
# 用pure ovs with offload
file=pkt_send_mul_auto_sta5_2
remotefile=pkt_rcv_mul_auto_sta3
lab=lab_simplex

line="bf3"
if [[ ${line} == "bf3" ]]
then
    arm_ip="10.15.198.164"
fi

send_host="node2"
recv_host="node1"
if [[ ${send_host} == "node2" ]]
then
    send_ip="10.15.198.161"
fi


user="qyn"
if [[ ${user} == "qyn" ]]
then
    run_path="/home/qyn/software/FastNIC/lab_new/${lab}"
    password="nesc77qq"
fi

if [[ ! -d "${run_path}/lab_results/ovslog" ]]
then
    mkdir -p ${run_path}/lab_results/ovslog
fi

if [[ ! -d "${run_path}/lab_results/log" ]]
then
    mkdir -p ${run_path}/lab_results/log
fi

core_id="0"
pkt_len=-1
flow_size=-1
srcip_num=-1
dstip_num=-1
zipf_para=-1

off_thre=-1

test_time_rcv=30
test_time_send=10

# flow_num_list=(100000 10000 100)
flow_num_list=(100 100000)
cir_time_fn=${#flow_num_list[@]}

times=0
for ((i=0; i<$cir_time_fn; i++))
do
    flow_num=${flow_num_list[$i]}

    # echo "expect remote_bf2_config.expect $off_thre"
    # expect remote_bf2_config.expect $off_thre

    send_run_para="flow_num $flow_num pkt_len $pkt_len flow_size $flow_size test_time $test_time_send srcip_num $srcip_num dstip_num $dstip_num zipf_para $zipf_para"
    rcv_run_para="flow_num $flow_num pkt_len -1 flow_size $flow_size test_time $test_time_rcv srcip_num $srcip_num dstip_num $dstip_num zipf_para -1"

    echo "expect remote_run_sta_bf3.expect $run_path $user $password $remotefile $line \"$rcv_run_para\" >> ../lab_results/log/remote.out 2>&1 &"
    expect remote_run_sta_bf3.expect $run_path $user $password $remotefile $line "$rcv_run_para" >> ../lab_results/log/remote.out 2>&1 &
    sleep 8s

    echo ./start_sta.sh $file $line $send_host $core_id $run_path \"$send_run_para\"
    ./start_sta.sh $file $line $send_host $core_id $run_path "$send_run_para"
    sleep 30s

    mkdir ${run_path}/lab_results/${file}/send_$times
    mv ${run_path}/lab_results/${file}/*.csv ../lab_results/${file}/send_$times/
    echo -e "off_thre,zipf_para\r\n${off_thre},${zipf_para}" > ${run_path}/lab_results/${file}/send_$times/para.csv

    ssh $user@$send_ip "cd $run_path && mkdir -p ./lab_results/${remotefile}/rcv_$times"
    ssh $user@$send_ip "cd $run_path/lab_results/${remotefile}/ && mv *csv rcv_$times/"
    
    ovsfile_path="/home/ubuntu/software/FastNIC/lab_results/ovs_log"
    mkdir ${run_path}/lab_results/ovslog/log_$times
    scp ubuntu@${arm_ip}:$ovsfile_path/*.csv $run_path/lab_results/ovslog/log_$times
    ssh ubuntu@${arm_ip} "cd $ovsfile_path && rm -f ./*.csv"
    ((times++))
done

