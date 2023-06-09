# adjust timestamp
ssh qyn@10.15.198.149 "sudo service ntp stop"
ssh qyn@10.15.198.149 "sudo ntpdate -v 10.15.198.150"
ssh qyn@10.15.198.149 "sudo service ntp start"
ssh ubuntu@10.15.198.148 "sudo service ntp stop"
ssh ubuntu@10.15.198.148 "sudo ntpdate -v 10.15.198.150"
ssh ubuntu@10.15.198.148 "sudo service ntp start"
date +%Y-%m-%d\ %H:%M:%S
ssh qyn@10.15.198.149 "date +%Y-%m-%d\ %H:%M:%S"
ssh ubuntu@10.15.198.148 "date +%Y-%m-%d\ %H:%M:%S"
