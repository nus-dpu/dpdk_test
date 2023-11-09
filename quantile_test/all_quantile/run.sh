#!/bin/bash
folder_path="../data"

# editcap -c 1000000 ../data/202308011400.pcap ../data/sample.pcap

rm all_quantile.csv
# Loop through files starting with "sample"
for file in "$folder_path"/sample*; do
    filename=$(basename "$file")
    echo ${filename}
    sed -i 's|#define PCAP_FILE_NAME.*$|#define PCAP_FILE_NAME "../data/'"${filename}"'"|' all_quantile.h

    make clean
    make
    ./build/all_quantile
done

