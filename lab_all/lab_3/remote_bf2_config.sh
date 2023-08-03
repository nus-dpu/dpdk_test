ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "sudo ~/bin/ovs-ctl restart --system-id=random"
ssh -A -t qyn@10.15.198.160 ssh ubuntu@192.168.100.2 "/home/ubuntu/software/FastNIC/lab_all/lab_3/myovs_rule_install.sh"

