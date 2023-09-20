# 均匀分布，不同发包侧core，不同flow number，不同pkt size，
# 用pure ovs with offload
sendfile=pkt_send_mul_auto_sta5_2
recvfile=pkt_rcv_mul_auto_sta3
lab=lab_simplex

# line="bf2"
# line="cx5"
line="nusbf2"

user="qyn"
if [[ ${user} == "qyn" ]]
then
    run_path="/local/qyn/software/FastNIC/lab_new/${lab}"
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
flow_num_list=(100000)
cir_time_fn=${#flow_num_list[@]}

times=0
for ((i=0; i<$cir_time_fn; i++))
do
    flow_num=${flow_num_list[$i]}

    # echo "expect remote_bf2_config.expect $off_thre"
    # expect remote_bf2_config.expect $off_thre

    send_run_para="flow_num $flow_num pkt_len $pkt_len flow_size $flow_size test_time $test_time_send srcip_num $srcip_num dstip_num $dstip_num zipf_para $zipf_para"
    rcv_run_para="flow_num $flow_num pkt_len 64 flow_size $flow_size test_time $test_time_rcv srcip_num $srcip_num dstip_num $dstip_num zipf_para -1"

    echo ./start_sta.sh $recvfile $line "node11" "16-31" $run_path \"$rcv_run_para\" >> ../lab_results/log/recv.out 2>&1 &
    ./start_sta.sh $recvfile $line "node11" "16-31" $run_path "$rcv_run_para" >> ../lab_results/log/recv.out 2>&1 &
    sleep 8s


    echo ssh 'qyn@nsl-node12.d2.comp.nus.edu.sg' "cd ${run_path}/script && ./start_sta.sh $sendfile $line node12 $core_id $run_path \"$send_run_para\" "
    ssh 'qyn@nsl-node12.d2.comp.nus.edu.sg' "cd ${run_path}/script && ./start_sta.sh $sendfile $line node12 $core_id $run_path \"$send_run_para\" "
    sleep 30s

    mkdir -p ${run_path}/lab_results/${sendfile}/send_$times
    mv ${run_path}/lab_results/${sendfile}/*.csv ${run_path}/lab_results/${sendfile}/send_$times/
    echo -e "off_thre,zipf_para\r\n${off_thre},${zipf_para}" > ${run_path}/lab_results/${sendfile}/send_$times/para.csv

    mkdir -p ${run_path}/lab_results/${recvfile}/rcv_$times
    mv ${run_path}/lab_results/${recvfile}/*.csv ${run_path}/lab_results/${recvfile}/rcv_$times/
    
    ovsfile_path="/home/ubuntu/software/FastNIC/lab_results/ovs_log"
    mkdir ${run_path}/lab_results/ovslog/log_$times
    scp ubuntu@192.168.100.2:$ovsfile_path/*.csv ${run_path}/lab_results/ovslog/log_$times
    ssh ubuntu@192.168.100.2 "cd $ovsfile_path && rm -f ./*.csv"
    ((times++))
done

