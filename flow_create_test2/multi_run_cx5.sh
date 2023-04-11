# run_path="/home/qyn/software/FastNIC"

rm -f ../lab_results/flow_create_test2/run_time*
make

for i in $(seq 1 20)
do
    rm -f ../lab_results/flow_create_test2/run_time.csv
    # sudo ./build/flow_create_test2 -l 0 -a 03:00.0
    sudo ./build/flow_create_test2 -l 0 -a af:00.0
    mv ../lab_results/flow_create_test2/run_time.csv ../lab_results/flow_create_test2/run_time_${i}.csv
done
