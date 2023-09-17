flow_num_list=(2000000 1000000 500000 200000)
flow_group_list=(200 100 500 200)

for ((test=0; test<${#flow_num_list[@]}; test++))
# for flow_num in {100000,1000,10000,20000,30000,40000,50000,60000,70000,80000,90000,100}
do
    flow_num=${flow_num_list[$test]}
    flow_group=${flow_group_list[$test]}

    for zipf_para in {1.1,1.2,1.3,1.5,1.7,2.0}
    do
        sed -i "s|flow_num =.*$|flow_num = ${flow_num}|" zipf_flow_gen2.py
        sed -i "s|flow_group =.*$|flow_group = ${flow_group}|" zipf_flow_gen2.py
        sed -i "s|zipf_para =.*$|zipf_para = ${zipf_para}|" zipf_flow_gen2.py
        python3 zipf_flow_gen2.py
    done
done