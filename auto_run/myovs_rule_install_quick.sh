sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl del-flows ovsdpdk
echo "finish del"

O_CIRCLE_NUM=10
I_CIRCLE_NUM=10000
for i in $(seq 0 $(($O_CIRCLE_NUM-1)))
do
  echo $i
  echo "/home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl add-flows ovsdpdk rule_$i.txt"
  sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl add-flows ovsdpdk rule_$i.txt
  echo "finish add $(($i*$I_CIRCLE_NUM)) - $((($i+1)*$I_CIRCLE_NUM-1))"
done