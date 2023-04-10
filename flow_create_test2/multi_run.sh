
for i in {1..3}
do
    rm -f ../lab_results/flow_create_test2/run_time.csv
    sudo ./build/flow_create_test2 -l 0 -a 03:00.0
    scp ../lab_results/flow_create_test2/run_time.csv qyn@192.168.100.1:/home/qyn/software/FastNIC/lab_results/flow_create_test2/run_time_${i}.csv
done
