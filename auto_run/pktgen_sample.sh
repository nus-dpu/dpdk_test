#149
sudo /home/qyn/software/dpdk/pktgen-dpdk/Builddir/app/pktgen -l 18-22 -n 4 -a d8:00.0 -- -P -m "[19-22].0" -T #bf2
sudo /home/qyn/software/dpdk/pktgen-dpdk/Builddir/app/pktgen -l 18-22 -n 4 -a af:00.0 -- -P -m "[19-22].0" -T #cx5
#150
sudo pktgen -l 0,2,4,6,8 -n 4 -a 5e:00.0 -- -P -m "[2/4/6/8].0" -T #cx4
#sudo pktgen -l 0,2,4,6,8 -n 4 -a 3b:00.0 -- -P -m "[2/4/6/8].0" -T #cx5
