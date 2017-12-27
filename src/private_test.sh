#!/bin/bash

set -xue

fifo_nums=(5582 11164 16746 22328 27910 33492 39074 44656 50238)

total_ssd=56863

total_fifo=10718

for i in "${!fifo_nums[@]}";

do
    rm -rf /dev/shm/*

    first_cache_num=$[$total_ssd*i/10+$total_ssd/10]
    second_cache_num=$[total_ssd-$first_cache_num]

    echo $i,$first_cache_num,$second_cache_num

    cgexec -g "blkio:fo1" ./smr-ssd-cache 0 0 0 0 0 $total_ssd $total_fifo $first_cache_num > part1_user0_test_$i.txt &

    cgexec -g "blkio:fo2" ./smr-ssd-cache 1 1 8 0 20000000 $total_ssd $total_fifo $second_cache_num > part1_user8_test_$i.txt &

    wait

done

total_ssd=55822

total_fifo=10485

for i in "${!fifo_nums[@]}";

do
    rm -rf /dev/shm/*

    first_cache_num=$[$total_ssd*i/10+$total_ssd/10]
    second_cache_num=$[total_ssd-$first_cache_num]

    echo $i,$first_cache_num,$second_cache_num

    cgexec -g "blkio:fo1" ./smr-ssd-cache 0 0 3 0 0 $total_ssd $total_fifo $first_cache_num > part2_user3_test_$i.txt &

    cgexec -g "blkio:fo2" ./smr-ssd-cache 1 1 8 0 20000000 $total_ssd $total_fifo $second_cache_num > part2_user8_test_$i.txt &

    wait

done
