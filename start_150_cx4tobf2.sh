file_name=$1 
echo ${file_name}
src_nic_name=ens1np0
dst_ip="192.168.200.1"

src_mac=`ifconfig ${src_nic_name}|grep ether|awk '{print $2}'`
dst_mac=`ar p ${dst_ip}|grep ether|awk '{print $3}'`
echo src_mac:${src_mac}
echo dst_mac:${dst_mac}

export PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig/
cd /home/qyn/software/FastNIC/$file_name/
make clean
make
sudo ./build/$file_name -l 0,1 -a 3b:00.0 -- --srcmac ${src_mac} --dstmac ${dst_mac} #bf2
# sudo ./build/$file_name -l 0 -a d8:00.0 #cx5

##odd core may have problems in cx4