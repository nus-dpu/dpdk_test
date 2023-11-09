# run_path="/home/qyn/software/FastNIC"
file_name="flow_sample"

if [[ ! -d "../lab_results/${file_name}" ]]
then
    mkdir ../lab_results/${file_name}
fi

rm ../lab_results/log/query_info.out
rm -f ../lab_results/${file_name}/run_time*
make

for i in $(seq 1 2)
do
    rm -f ./run_time.csv
    sudo ./build/${file_name} -l 31 -a 3b:00.0
    mv ../lab_results/${file_name}/run_time.csv ../lab_results/${file_name}/run_time_${i}.csv
    echo "finish test ${i}"
done
