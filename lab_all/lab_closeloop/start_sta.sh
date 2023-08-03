file_name=$1 
line=$2
host_name=$3
core_id=$4
flow_num=$5
pkt_len=$6
flow_size=$7
test_time=$8 #run_time = test_time * 0.5s
run_path=$9

# determine nic, ip, mac
if [[ $line == "bf2" && $host_name == 160 ]] #bf2-cx5
then 
    src_nic_name="ens5f1np1"
    dst_ip="192.168.200.2"
    dst_mac="08:c0:eb:de:43:36"
elif [[ $line == "bf2" && $host_name == 161 ]] #cx5-bf2
then 
    src_nic_name="ens28np0"
    dst_ip="192.168.200.1"
    dst_mac="0c:42:a1:d8:10:85"
# elif [[ $line == "cx5" && $host_name == 149 ]] #cx5(withbf2)-cx5(withcx4)
# then 
#     src_nic_name="enp175s0"
#     dst_ip="192.168.201.2"
#     dst_mac="08:c0:eb:de:43:2e"
# elif [[ $line == "cx5" && $host_name == 150 ]] #cx5(withbf4)-cx5(withcx5)
# then 
#     src_nic_name="ens3np0"
#     dst_ip="192.168.201.1"
#     dst_mac="08:c0:eb:de:41:f2"
fi
src_mac=`ifconfig ${src_nic_name}|grep ether|awk '{print $2}'`
src_pci=`ethtool -i ${src_nic_name}|grep bus-info|awk '{print $2}'`
# dst_mac=`arp ${dst_ip}|grep ether|awk '{print $3}'`

echo run ${file_name} app, from ${src_nic_name} to ${dst_ip}
echo src_mac:${src_mac},src_pci:${src_pci}
echo dst_mac:${dst_mac}
echo -e '\n'

# if [[ ${host_name} == "149" ]]
# then
#     export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig/ #149
# elif [[ ${host_name} == "150" ]]
# then
#     export PKG_CONFIG_PATH=/usr/local/lib/x86_64-linux-gnu/pkgconfig/ #150
# fi
export PKG_CONFIG_PATH=/usr/local/lib/x86_64-linux-gnu/pkgconfig/

cd $run_path/$file_name/

if [[ ! -d "../lab_results/${file_name}" ]]
then
    mkdir ../lab_results/${file_name}
fi

echo "sudo ./build/$file_name -l ${core_id} -a ${src_pci} -- --srcmac ${src_mac} --dstmac ${dst_mac}"
if [[ ${file_name} == "pkt_send_mul_auto_sta" || \
      ${file_name} == "pkt_send_mul_auto_sta2" ]]
then
    sed -i "s/#define FLOW_NUM.*$/#define FLOW_NUM ${flow_num}/" para.h
    sed -i "s/#define PKT_LEN.*$/#define PKT_LEN ${pkt_len}/" para.h
    sed -i "s/#define MAX_RECORD_COUNT.*$/#define MAX_RECORD_COUNT ${test_time}/" para.h
    make clean
    make
    sudo ./build/$file_name -l ${core_id} -a ${src_pci} -- --srcmac ${src_mac} --dstmac ${dst_mac} 
elif [[ ${file_name} == "pkt_send_mul_auto_sta3" || \
        ${file_name} == "pkt_loopsend_mul_sta" ]]
then
    sed -i "s/#define FLOW_NUM.*$/#define FLOW_NUM ${flow_num}/" para.h
    sed -i "s/#define PKT_LEN.*$/#define PKT_LEN ${pkt_len}/" para.h
    sed -i "s/#define MAX_RECORD_COUNT.*$/#define MAX_RECORD_COUNT ${test_time}/" para.h
    sed -i "s/#define FLOW_SIZE.*$/#define FLOW_SIZE ${flow_size}/" para.h
    make clean
    make
    sudo ./build/$file_name -l ${core_id} -a ${src_pci} -- --srcmac ${src_mac} --dstmac ${dst_mac} 
elif [[ ${file_name} == "pkt_rcv_mul_auto_sta" || \
        ${file_name} == "pkt_rcv_mul_auto_sta3" || \
        ${file_name} == "pkt_looprcv_mul_sta" ]]
then
    sed -i "s/#define MAX_RECORD_COUNT.*$/#define MAX_RECORD_COUNT ${test_time}/" para.h
    make clean
    make
    sudo ./build/$file_name -l ${core_id} -a ${src_pci}
fi
