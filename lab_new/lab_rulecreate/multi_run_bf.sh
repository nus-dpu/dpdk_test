lab=flow_create_test2
flow_num=100000
group_id=0

if [[ ! -d "./lab_results/${lab}" ]]; then
    mkdir -p ./lab_results/${lab}
fi

cd ${lab}
sed -i "s/#define FLOW_NUM.*$/#define FLOW_NUM ${flow_num}/" para.h
sed -i "s/#define GROUP_ID.*$/#define GROUP_ID ${group_id}/" para.h
make clean && make

for i in $(seq 1 3)
do
    rm -f ../lab_results/${lab}/run_time.csv
    sudo ./build/flow_create_test2 -l 0 -a 03:00.0
    mv ../lab_results/${lab}/run_time.csv ../lab_results/${lab}/run_time_${i}.csv
done
