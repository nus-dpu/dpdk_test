ip_prefix=$((192<<24))

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

#get the last position of 1 in a interger's binary form
function get_last_one_position() {
  local num=$1
  local position=0

  while [[ $num -ne 0 ]]; do
    if (( num & 1 == 1 )); then
      echo $position
      return
    fi

    (( num >>= 1 ))
    (( position++ ))
  done
}

#get the highest position of 1 in a interger's binary form
get_highest_one_position() {
  local num=$1
  local position=0

  while [[ $num -ne 0 ]]; do
    (( num >>= 1 ))
    (( position++ ))
  done

  (( position -= 1 ))
  
  echo $position
}

sudo ovs-ofctl del-flows ovsdpdk
sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl del-flows ovsdpdk
echo "finish del"

O_CIRCLE_NUM=20
I_CIRCLE_NUM=5000
for((i=0;i<$O_CIRCLE_NUM;i++));
do
  rm rule_$i.txt
  echo "del rule_$i.txt"
  for((j=0;j<I_CIRCLE_NUM;j++));
  do
    ip=$(($ip_prefix+$j+$i*$I_CIRCLE_NUM))

    if [[ ${i} == 0 && ${j} == 0 ]]
    then
      result=$(get_highest_one_position $(($O_CIRCLE_NUM * $I_CIRCLE_NUM)))
      rule_wc=$((32-$result))
      for((k=32;k>=$rule_wc;k--));
      do
        ip_dot=`num2ip $ip`
        table=$((32-$k))
        if [[ ${k} == 32 ]]
        then
          actions="output:dpdk_p0"
          echo "table=$table,ip,in_port=dpdk_p0hpf,ip_src=$ip_dot/$k,priority=$k,actions=${actions}" >> rule_$i.txt
        else
          actions="resubmit(,$((${table} - 1)))"
          echo "table=$table,ip,in_port=dpdk_p0hpf,ip_src=$ip_dot/$k,priority=$k,actions=${actions}" >> rule_$i.txt
        fi
      done
    else
      result=$(get_last_one_position $ip)
      rule_wc=$((32-$result))
      for((k=32;k>=$rule_wc;k--));
      do
        ip_dot=`num2ip $ip`
        table=$((32-$k))
        if [[ ${k} == 32 ]]
        then
          actions="output:dpdk_p0"
          echo "table=$table,ip,in_port=dpdk_p0hpf,ip_src=$ip_dot/$k,priority=$k,actions=${actions}" >> rule_$i.txt
        else
          actions="resubmit(,$((${table} - 1)))"
          echo "table=$table,ip,in_port=dpdk_p0hpf,ip_src=$ip_dot/$k,priority=$k,actions=${actions}" >> rule_$i.txt
        fi
      done
    fi

    # if [[ $(($j % 1000)) == 0 ]];
    # then
    #   echo $ip_dot
    # fi
  done
  sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl add-flows ovsdpdk rule_$i.txt
  echo "finish add $(($i*$I_CIRCLE_NUM)) - $((($i+1)*$I_CIRCLE_NUM-1))"
done