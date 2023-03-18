file_name=$1 
src_nic_name=$2
dst_ip=$3
core_id=$4
flow_num=$5
pkt_len=$6
test_time=$7 #run_time = test_time * 0.5s
echo run ${file_name} app, from ${src_nic_name} to ${dst_ip}

src_mac=`ifconfig ${src_nic_name}|grep ether|awk '{print $2}'`
src_pci=`ethtool -i ${src_nic_name}|grep bus-info|awk '{print $2}'`
# dst_mac=`arp ${dst_ip}|grep ether|awk '{print $3}'`
if [[ ${dst_ip} == "192.168.200.2" ]] #150,cx4
then
    dst_mac="b8:83:03:82:a2:10"
elif [[ ${dst_ip} == "192.168.201.2" ]] #150,cx5
then
    dst_mac="08:c0:eb:de:43:2e"
elif [[ ${dst_ip} == "192.168.200.1" ]] #149,bf2
then
    dst_mac="0c:42:a1:d8:10:84"
elif [[ ${dst_ip} == "192.168.201.1" ]] #149,cx5
then
    dst_mac="08:c0:eb:de:41:f2"
fi

echo src_mac:${src_mac},src_pci:${src_pci}
echo dst_mac:${dst_mac}
echo -e '\n'

export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig/ #149
cd /home/qyn/software/FastNIC/$file_name/
make clean
make

if [[ ! -d "../lab_results/${file_name}" ]]
then
    mkdir ../lab_results/${file_name}
fi

if [[ ${file_name} == "pkt_send_mul_auto_sta" ]]
then
    sed -i "s/#define FLOW_NUM.*$/#define FLOW_NUM ${flow_num}/" para.h
    sed -i "s/#define PKT_LEN.*$/#define PKT_LEN ${pkt_len}/" para.h
    sed -i "s/#define MAX_RECORD_COUNT.*$/#define MAX_RECORD_COUNT ${test_time}/" para.h
    sudo ./build/$file_name -l ${core_id} -a ${src_pci} -- --srcmac ${src_mac} --dstmac ${dst_mac} 
elif [[ ${file_name} == "pkt_rcv_mul_auto_sta" ]]
then
    sed -i "s/#define MAX_RECORD_COUNT.*$/#define MAX_RECORD_COUNT ${test_time}/" para.h
    sudo ./build/$file_name -l ${core_id} -a ${src_pci}
fi
