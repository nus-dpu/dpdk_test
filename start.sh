file_name=$1 
src_nic_name=$2
dst_ip=$3
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

export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig/
cd /home/qyn/software/FastNIC/$file_name/
make clean
make
sudo ./build/$file_name -l 0,1 -a ${src_pci} -- --srcmac ${src_mac} --dstmac ${dst_mac} #bf2
