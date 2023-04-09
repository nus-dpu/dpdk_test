rm ./lab_results/log/remote.out
rm ./lab_results/log/bf.out
rm -f ./lab_results/$file/throughput*
ssh qyn@10.15.198.150 "cd $run_path && rm -f ./lab_results/$remotefile/throughput*"
echo "del former file successfully"
