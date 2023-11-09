file_name=flow_create_test3

if [[ ! -d "../lab_results/${file_name}" ]]
then
    mkdir -p ../lab_results/${file_name}
fi

rm -f ../lab_results/${file_name}/*
echo "finish del"

i=0
sed -i "s/#define GROUP_ID.*$/#define GROUP_ID 0/" para.h
for base_flow_num in {6554,13107,19661,26214,32768,39321,45875,52428,58982,65535}
do
    sed -i "s/#define BASE_FLOW_NUM.*$/#define BASE_FLOW_NUM ${base_flow_num}/" para.h
    make clean
    make
    sudo ./build/${file_name} -l 0 -a 0000:17:00.0
    mv ../lab_results/${file_name}/run_time.csv ../lab_results/${file_name}/run_time_group0_${i}.csv
    ((i++))
done

i=0
sed -i "s/#define GROUP_ID.*$/#define GROUP_ID 1/" para.h
for base_flow_num in {1e6,2e6,3e6,4e6,5e6,6e6,7e6,8e6,9e6,1e7}
do
    sed -i "s/#define BASE_FLOW_NUM.*$/#define BASE_FLOW_NUM ${base_flow_num}/" para.h
    make clean
    make
    sudo ./build/${file_name} -l 0 -a 0000:17:00.0
    mv ../lab_results/${file_name}/run_time.csv ../lab_results/${file_name}/run_time_group1_${i}.csv
    ((i++))
done

