#!/bin/bash

set -xue

fifo_nums=(5582 11164 16746 22328 27910 33492 39074 44656 50238)

total_ssd=55822

total_fifo=10485

for i in "${!fifo_nums[@]}";

do
    rm -rf /dev/shm/*

    ssd_cache_num=$[total_ssd-${fifo_nums[$i]}]

    echo $i,${fifo_nums[$i]},$ssd_cache_num

    cgexec -g "blkio:fo1" ./smr-ssd-cache 0 0 3 0 0 $total_ssd $total_fifo ${fifo_nums[$i]} > user3_test_$i.txt &

    cgexec -g "blkio:fo2" ./smr-ssd-cache 1 1 8 20000000 0 $total_ssd $total_fifo $ssd_cache_num > user8_test_$i.txt &

    wait

done
