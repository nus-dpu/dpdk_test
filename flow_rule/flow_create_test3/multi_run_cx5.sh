file_name=flow_create_test3

if [[ ! -d "../lab_results/${file_name}" ]]
then
    mkdir -p ../lab_results/${file_name}
fi

rm -f ../lab_results/${file_name}/*
echo "finish del"

i=0
for base_flow_num in {6554,13107,19661,26214,32768,39321,45875,52428,58982,65535}
do
    sed -i "s/#define BASE_FLOW_NUM.*$/#define BASE_FLOW_NUM ${base_flow_num}/" para.h
    make clean
    make
    sudo ./build/${file_name} -l 0 -a 0000:17:00.0
    mv ../lab_results/${file_name}/run_time.csv ../lab_results/${file_name}/run_time_${i}.csv
    ((i++))
done
