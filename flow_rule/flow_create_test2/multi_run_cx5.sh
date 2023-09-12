file_name="flow_create_test2"

if [[ ! -d "../lab_results/${file_name}" ]]
then
    mkdir -p ../lab_results/${file_name}
fi

# rm -f ../lab_results/${file_name}/*.csv
# make clean
# make

# for i in $(seq 1 20)
# do
#     rm -f ../lab_results/${file_name}/run_time.csv
#     sudo ./build/${file_name} -l 0 -a 0000:17:00.0
#     mv ../lab_results/${file_name}/run_time.csv ../lab_results/${file_name}/run_time_${i}.csv
# done

rm -f ../lab_results/${file_name}/*.csv

sed -i "s/#define GROUP_ID.*$/#define GROUP_ID 0/" para.h
make clean
make
sudo ./build/${file_name} -l 0 -a 0000:17:00.0 #160 cx5
mv ../lab_results/${file_name}/run_time.csv ../lab_results/${file_name}/run_time_group0.csv

sed -i "s/#define GROUP_ID.*$/#define GROUP_ID 1/" para.h
make clean
make
sudo ./build/${file_name} -l 0 -a 0000:17:00.0 #160 cx5
mv ../lab_results/${file_name}/run_time.csv ../lab_results/${file_name}/run_time_group1.csv

sed -i "s/#define GROUP_ID.*$/#define GROUP_ID 2/" para.h
make clean
make
sudo ./build/${file_name} -l 0 -a 0000:17:00.0 #160 cx5
mv ../lab_results/${file_name}/run_time.csv ../lab_results/${file_name}/run_time_group2.csv