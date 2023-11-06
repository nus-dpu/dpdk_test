#!/bin/bash
folder_path="../data/synthetic/"

# editcap -c 1000000 ../data/202308011400.pcap ../data/sample.pcap

for file in "$folder_path"/*; do
    filename=$(basename "$file")
    echo ${filename}
    sed -i 's|#define PCAP_FILE_NAME.*$|#define PCAP_FILE_NAME "../data/synthetic/'"${filename}"'"|' data_transfer.h

    make clean
    make
    ./build/data_transfer

    mkdir -p ./build/${filename}
    mv ./build/all_flows.csv ./build/${filename}
    mv ./build/tcpudp_flows.csv ./build/${filename}
    mv ./build/tui_flows.csv ./build/${filename}

done



