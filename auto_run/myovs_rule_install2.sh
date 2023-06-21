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
  rm src_rule_$i.txt
  echo "del src_rule_$i.txt"
  for((j=0;j<$SRC_I_CIRCLE_NUM;j++));
  do
    src_ip=$(($src_ip_prefix+$j+$i*$SRC_I_CIRCLE_NUM))
    src_ip_dot=`num2ip $src_ip`
    if [[ $(($j % 100)) == 0 ]];
    then
      echo $src_ip_dot
    fi
    echo "table=0,ip,in_port=dpdk_p0,ip_src=$src_ip_dot,actions=resubmit(,1)" >> src_rule_$i.txt
  done
  sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl add-flows ovsdpdk src_rule_$i.txt
  echo "finish add $(($i*$SRC_I_CIRCLE_NUM)) - $((($i+1)*$SRC_I_CIRCLE_NUM-1))"
done
echo "finish src add"

#dst
echo "start dst add"
for((i=0;i<$DST_O_CIRCLE_NUM;i++));
do
  rm dst_rule_$i.txt
  echo "del dst_rule_$i.txt"
  for((j=0;j<$DST_I_CIRCLE_NUM;j++));
  do
    dst_ip=$(($dst_ip_prefix+$j+$i*$DST_I_CIRCLE_NUM))
    dst_ip_dot=`num2ip $dst_ip`
    if [[ $(($j % 100)) == 0 ]];
    then
      echo $dst_ip_dot
    fi
    echo "table=1,ip,ip_dst=$dst_ip_dot,actions=output:dpdk_p0hpf" >> dst_rule_$i.txt
  done
  sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl add-flows ovsdpdk dst_rule_$i.txt
  echo "finish add $(($i*$DST_I_CIRCLE_NUM)) - $((($i+1)*$DST_I_CIRCLE_NUM-1))"
done
echo "finish dst add"
