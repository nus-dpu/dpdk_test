# file=pkt_send_mul_auto_sta4
# file=pkt_send_mul_auto_sta5_2
file=pkt_send_mul_auto_sta4_2

remotefile=pkt_rcv_mul_auto_sta3
lab=lab_5

line="bf2"
# line="cx5"

user="qyn"
if [[ ${user} == "cz" ]]
then
    run_path="/home/cz/3_20/FastNIC"
    password="123456"
elif [[ ${user} == "qyn" ]]
then
    run_path="/home/qyn/software/FastNIC/lab_all/${lab}"
    password="nesc77qq"
fi


# core_id="0,2,4,6,8,10,12,14,16,18,20,22,24,26,28,30"
core_id="0"
pkt_len=-1
flow_size=-1
srcip_num=-1
dstip_num=-1
# test_time_rcv=80
# test_time_send=60
test_time_rcv=80
test_time_send=60

if [[ ! -d "./lab_results/ovslog" ]]
then
    mkdir -p ./lab_results/ovslog
fi

if [[ ! -d "./lab_results/log" ]]
then
    mkdir -p ./lab_results/log
fi

# 用off2，pkt_send_mul_auto_sta5_2和pkt_send_mul_auto_sta4测试不同offload thre下面吞吐量的效果
i=0
for off_thre in {1,2,3,4,5,7,10,20,30}
do
    # for flow_num in {100,1000,10000,20000,30000,40000,50000,60000,70000,80000,90000,100000}
    for flow_num in {100000,100}
    do
        for zipf_para in {1.1,1.2,1.3,1.5,1.7,2.0}
        do
            echo "expect remote_bf2_config.expect $off_thre"
            expect remote_bf2_config.expect $off_thre

            send_run_para="flow_num $flow_num pkt_len $pkt_len flow_size $flow_size test_time $test_time_send srcip_num $srcip_num dstip_num $dstip_num zipf_para $zipf_para"
            rcv_run_para="flow_num $flow_num pkt_len 64 flow_size $flow_size test_time $test_time_rcv srcip_num $srcip_num dstip_num $dstip_num zipf_para -1"

            echo "expect remote_run_sta_cx4.expect $run_path $user $password $remotefile $line \"$rcv_run_para\" >> ./lab_results/log/remote.out 2>&1 &"
            remote_run_sta_cx4.expect $run_path $user $password $remotefile $line "$rcv_run_para" >> ./lab_results/log/remote.out 2>&1 &
            sleep 8s

            echo ./start_sta.sh $file $line 150 $core_id $run_path \"$send_run_para\"
            ./start_sta.sh $file $line 150 $core_id $run_path "$send_run_para" #149,bf2tocx4
            sleep 30s

            mkdir ./lab_results/${file}/send_$i
            mv ./lab_results/${file}/*.csv ./lab_results/${file}/send_$i/
            echo -e "off_thre,zipf_para\r\n${off_thre},${zipf_para}" > ./lab_results/${file}/send_$i/para.csv

            ssh qyn@10.15.198.149 "cd $run_path && mkdir ./lab_results/${remotefile}/rcv_$i"
            ssh qyn@10.15.198.149 "cd $run_path/lab_results/${remotefile}/ && mv *csv rcv_$i/"
            
            ovsfile_path="/home/ubuntu/software/FastNIC/lab_results/ovs_log"
            mkdir ./lab_results/ovslog/log_$i
            scp ubuntu@10.15.198.148:$ovsfile_path/*.csv $run_path/lab_results/ovslog/log_$i
            ssh ubuntu@10.15.198.148 "cd $ovsfile_path && rm -f ./*.csv"
            ((i++))
        done
    done
done


# core_id="0"
# pkt_len=-1
# flow_size=-1
# srcip_num=-1
# dstip_num=-1

# test_time_rcv=30
# test_time_send=10

# if [[ ! -d "./lab_results/ovslog" ]]
# then
#     mkdir -p ./lab_results/ovslog
# fi

# if [[ ! -d "./lab_results/log" ]]
# then
#     mkdir -p ./lab_results/log
# fi

# # 用off2，pkt_send_mul_auto_sta5_2和pkt_send_mul_auto_sta4测试不同offload thre下面吞吐量的效果
# i=0
# off_thre=2
# flow_num=100
# zipf_para=1.1

# # echo "expect remote_bf2_config.expect $off_thre"
# # expect remote_bf2_config.expect $off_thre

# send_run_para="flow_num $flow_num pkt_len $pkt_len flow_size $flow_size test_time $test_time_send srcip_num $srcip_num dstip_num $dstip_num zipf_para $zipf_para"
# rcv_run_para="flow_num $flow_num pkt_len 64 flow_size $flow_size test_time $test_time_rcv srcip_num $srcip_num dstip_num $dstip_num zipf_para -1"

# echo "expect remote_run_sta_cx4.expect $run_path $user $password $remotefile $line \"$rcv_run_para\" >> ./lab_results/log/remote.out 2>&1 &"
# expect remote_run_sta_cx4.expect $run_path $user $password $remotefile $line "$rcv_run_para" >> ./lab_results/log/remote.out 2>&1 &
# sleep 8s

# echo ./start_sta.sh $file $line 150 $core_id $run_path \"$send_run_para\"
# ./start_sta.sh $file $line 150 $core_id $run_path "$send_run_para" #149,bf2tocx4
# sleep 30s

# mkdir ./lab_results/${file}/send_$i
# mv ./lab_results/${file}/*.csv ./lab_results/${file}/send_$i/
# echo -e "off_thre,zipf_para\r\n${off_thre},${zipf_para}" > ./lab_results/${file}/send_$i/para.csv

# ssh qyn@10.15.198.149 "cd $run_path && mkdir ./lab_results/${remotefile}/rcv_$i"
# ssh qyn@10.15.198.149 "cd $run_path/lab_results/${remotefile}/ && mv *csv rcv_$i/"

# ovsfile_path="/home/ubuntu/software/FastNIC/lab_results/ovs_log"
# mkdir ./lab_results/ovslog/log_$i
# scp ubuntu@10.15.198.148:$ovsfile_path/*.csv $run_path/lab_results/ovslog/log_$i
# ssh ubuntu@10.15.198.148 "cd $ovsfile_path && rm -f ./*.csv"

