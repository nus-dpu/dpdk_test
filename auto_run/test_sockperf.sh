#/bin/bash

basePort=12345
num=$10
for((i=0;i<$num;++i))
do
#    sockperf tp -i 192.168.200.2 --client_port $[${basePort}+${i}] --pps max -m 14 -t 3000 --port  $[${basePort}+${i}]  --sender-affinity $(($i%20)) &
    sockperf tp -i 192.168.200.2 -p $[${basePort}+${i}] -m 1472 --pps max -t 300
done
