#!/bin/bash
folder_path="../data"


editcap -c 1000000 ../data/202308011400.pcap ../data/sample.pcap

# Loop through files starting with "sample"
for file in "$folder_path"/sample*; do
    for percentage in {0.9,0.8,0.7}; do
        filename=$(basename "$file")
        echo ${filename}
        sed -i 's|#define PCAP_FILE_NAME.*$|#define PCAP_FILE_NAME "../data/'"${filename}"'"|' simple_quantile.h
        sed -i "s/#define QUANTILE_PER.*$/#define QUANTILE_PER ${percentage}/" simple_quantile.h

        make clean
        make
        ./simple_quantile
    done
done

