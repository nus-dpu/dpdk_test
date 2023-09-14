src_ip_prefix=$((192<<24))
dst_ip_prefix=$((193<<24))

function num2ip()
{
    num=$1
    a=$((num>>24))
    b=$((num>>16&0xff))
    c=$((num>>8&0xff))
    d=$((num&0xff))
 
    echo "$a.$b.$c.$d"
    # echo "$d.$c.$b.$a"

    return 0
}
 
function ip2num()
{
    ip=$1
    a=$(echo $ip | awk -F'.' '{print $1}')
    b=$(echo $ip | awk -F'.' '{print $2}')
    c=$(echo $ip | awk -F'.' '{print $3}')
    d=$(echo $ip | awk -F'.' '{print $4}')
 
    echo "$(((a << 24) + (b << 16) + (c << 8) + d))"
}

#sudo ovs-ofctl del-flows ovsdpdk
sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl del-flows ovsdpdk
echo "finish del"

SRC_IP_NUM=100000
SRC_O_CIRCLE_NUM=10
SRC_I_CIRCLE_NUM=10000
DST_IP_NUM=100000
DST_O_CIRCLE_NUM=10
DST_I_CIRCLE_NUM=10000

#src
echo "start src add"
for((i=0;i<$SRC_O_CIRCLE_NUM;i++));
do
  sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl add-flows ovsdpdk src_rule_$i.txt
  echo "finish add $(($i*$SRC_I_CIRCLE_NUM)) - $((($i+1)*$SRC_I_CIRCLE_NUM-1))"
done
echo "finish src add"

#dst
echo "start dst add"
for((i=0;i<$DST_O_CIRCLE_NUM;i++));
do
  sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl add-flows ovsdpdk dst_rule_$i.txt
  echo "finish add $(($i*$DST_I_CIRCLE_NUM)) - $((($i+1)*$DST_I_CIRCLE_NUM-1))"
done
echo "finish dst add"
