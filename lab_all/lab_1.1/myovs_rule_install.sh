src_ip_prefix=$((192<<24))
dst_ip=$(( (192<<24) + (168<<16) + (200<<8) + 1))
echo ${dst_ip_prefix}
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

O_CIRCLE_NUM=1
I_CIRCLE_NUM=1
for((i=0;i<$O_CIRCLE_NUM;i++));
do
  rm rule_$i.txt
  echo "del rule_$i.txt"
  for((j=0;j<I_CIRCLE_NUM;j++));
  do
    src_ip=$(($src_ip_prefix+$j+$i*$I_CIRCLE_NUM))
    src_ip_dot=`num2ip $src_ip`
    dst_ip_dot=`num2ip $dst_ip`

    if [[ $(($j % 1000)) == 0 ]];
    then
      echo $src_ip_dot
    fi

    #150 as send, 149 as recv
    echo "ip,in_port=dpdk_p0,ip_src=$src_ip_dot,ip_dst=$dst_ip_dot,actions=output:dpdk_p0hpf" >> rule_$i.txt
    echo "ip,in_port=dpdk_p0hpf,ip_src=$dst_ip_dot,ip_dst=$src_ip_dot,actions=output:dpdk_p0" >> rule_$i.txt
  done
  sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl add-flows ovsdpdk rule_$i.txt
  echo "finish add $(($i*$I_CIRCLE_NUM)) - $((($i+1)*$I_CIRCLE_NUM-1))"
done