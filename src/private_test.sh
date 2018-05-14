#!/bin/bash

set -xue

nine=(5582 11164 16746 22328 27910 33492 39074 44656 50238)

one=(5582)

total_ssd=56863

total_fifo=10718

echo "8:16 2048000" > /sys/fs/cgroup/blkio/fo1/blkio.throttle.read_bps_device
echo "8:16 2048000" > /sys/fs/cgroup/blkio/fo1/blkio.throttle.write_bps_device
echo "8:16 10240000" > /sys/fs/cgroup/blkio/fo2/blkio.throttle.read_bps_device
echo "8:16 10240000" > /sys/fs/cgroup/blkio/fo2/blkio.throttle.write_bps_device

rm -rf /dev/shm/*

cgexec -g "blkio:fo1" ./smr-ssd-cache 0 0 0 0 0 $total_ssd $total_fifo 0 0 > part31_user0_global.txt &

cgexec -g "blkio:fo2" ./smr-ssd-cache 1 1 8 0 20000000 $total_ssd $total_fifo 0 0 > part31_user1_global.txt &

wait


for i in "${!nine[@]}"

do
    rm -rf /dev/shm/*

    first_cache_num=$[$total_ssd*i/10]
    second_cache_num=$[total_ssd-$first_cache_num]

    echo $i,$first_cache_num,$second_cache_num

    cgexec -g "blkio:fo1" ./smr-ssd-cache 0 0 0 0 0 $total_ssd $total_fifo $first_cache_num 1 > part31_user0_test$i.txt &

    cgexec -g "blkio:fo2" ./smr-ssd-cache 1 1 8 0 20000000 $total_ssd $total_fifo $second_cache_num 1 > part31_user1_test$i.txt &


    wait

done

total_ssd=55822

total_fifo=10485

rm -rf /dev/shm/*

first_cache_num=$[$total_ssd*i/10]
second_cache_num=$[total_ssd-$first_cache_num]

cgexec -g "blkio:fo1" ./smr-ssd-cache 0 0 3 0 0 $total_ssd $total_fifo 0 0 > part32_user0_global.txt &

cgexec -g "blkio:fo2" ./smr-ssd-cache 1 1 8 0 20000000 $total_ssd $total_fifo 0 0 > part32_user1_global.txt &

wait


for i in "${!nine[@]}"

do
    rm -rf /dev/shm/*

    first_cache_num=$[$total_ssd*i/10]
    second_cache_num=$[total_ssd-$first_cache_num]

    echo $i,$first_cache_num,$second_cache_num

    cgexec -g "blkio:fo1" ./smr-ssd-cache 0 0 3 0 0 $total_ssd $total_fifo $first_cache_num 1 > part32_user0_test$i.txt &

    cgexec -g "blkio:fo2" ./smr-ssd-cache 1 1 8 0 20000000 $total_ssd $total_fifo $second_cache_num 1 > part32_user1_test$i.txt &


    wait

done
