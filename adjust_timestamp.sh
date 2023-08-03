# adjust timestamp
ssh qyn@10.15.198.160 "sudo service ntp stop"
ssh qyn@10.15.198.160 "sudo ntpdate -v 10.15.198.150"
ssh qyn@10.15.198.160 "sudo service ntp start"
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "sudo service ntp stop"
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "sudo ntpdate -v 10.15.198.150"
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "sudo service ntp start"
date +%Y-%m-%d\ %H:%M:%S
ssh qyn@10.15.198.160 "date +%Y-%m-%d\ %H:%M:%S"
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "date +%Y-%m-%d\ %H:%M:%S"
