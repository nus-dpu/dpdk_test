sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl del-flows ovsdpdk
echo "finish del"

O_CIRCLE_NUM=10
I_CIRCLE_NUM=10000
for((i=0;i<$O_CIRCLE_NUM;i++));
do
  sudo /home/ubuntu/software/ovs_all/ovs_install/usr/bin/ovs-ofctl add-flows ovsdpdk rule_$i.txt
  echo "finish add $(($i*$I_CIRCLE_NUM)) - $((($i+1)*$I_CIRCLE_NUM-1))"
done