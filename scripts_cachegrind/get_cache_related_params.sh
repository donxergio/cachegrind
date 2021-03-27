#!/bin/bash

#Extract the number of instructions, number of data accesses, number of cache misses from the data files generated by cachegrind


BENCHMARK_DIR=$1
benchmark_name=$2

SIZES=(256 512 1024 2048 4096 8192 16384 32768 65536)

cd $BENCHMARK_DIR
log_filename="stats_"$benchmark_name".log"
output_file=$benchmark_name"_results.log"

#echo "SIZE LRU-I LRU-D LRU-M LIP-I LIP-D LIP-M RANDOM-I RANDOM-D RANDOM-M FIFO-I FIFO-D FIFO-M" > $output_file
printf "%-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n" "SIZE" "LRU-I" "LRU-D" "LRU-M" "LIP-I" "LIP-D" "LIP-M" "RANDOM-I" "RANDOM-D" "RANDOM-M" "FIFO-I" "FIFO-D" "FIFO-M" > $output_file


for s in ${SIZES[@]}
do

    filename_lru=$(echo results_$benchmark_name\_$s\_lru.log)
    filename_lip=$(echo results_$benchmark_name\_$s\_lip.log)
    filename_random=$(echo results_$benchmark_name\_$s\_random.log)
    filename_fifo=$(echo results_$benchmark_name\_$s\_fifo.log)

    irefs_lru=$(cat $filename_lru | grep "I   refs:" | awk '{print $4}' |  sed s/,//g)
    drefs_lru=$(cat $filename_lru | grep "D   refs:" | awk '{print $4}' |  sed s/,//g)
    dmiss_lru=$(cat $filename_lru | grep "D1  misses" | awk '{print $4}' |  sed s/,//g)

    echo "LRU: Number of instructions: $irefs_lru, Data accesses: $drefs_lru, Number of misses: $dmiss_lru"

    irefs_lip=$(cat $filename_lip | grep "I   refs:" | awk '{print $4}' |  sed s/,//g)
    drefs_lip=$(cat $filename_lip | grep "D   refs:" | awk '{print $4}' |  sed s/,//g)
    dmiss_lip=$(cat $filename_lip | grep "D1  misses" | awk '{print $4}' |  sed s/,//g)

    echo "LIP: Number of instructions: $irefs_lip, Data accesses: $drefs_lip, Number of misses: $dmiss_lip"

    irefs_rand=$(cat $filename_random | grep "I   refs:" | awk '{print $4}' |  sed s/,//g)
    drefs_rand=$(cat $filename_random | grep "D   refs:" | awk '{print $4}' |  sed s/,//g)
    dmiss_rand=$(cat $filename_random | grep "D1  misses" | awk '{print $4}' |  sed s/,//g)

    echo "RANDOM: Number of instructions: $irefs_rand, Data accesses: $drefs_rand, Number of misses: $dmiss_rand"

    irefs_fifo=$(cat $filename_fifo | grep "I   refs:" | awk '{print $4}' |  sed s/,//g)
    drefs_fifo=$(cat $filename_fifo | grep "D   refs:" | awk '{print $4}' |  sed s/,//g)
    dmiss_fifo=$(cat $filename_fifo | grep "D1  misses" | awk '{print $4}' |  sed s/,//g)

    echo "FIFO: Number of instructions: $irefs_fifo, Data accesses: $drefs_fifo, Number of misses: $dmiss_fifo"

    printf "%-10d %-10d %-10d %-10d %-10d %-10d %-10d %-10d %-10d %-10d %-10d %-10d %-10d\n" $s $irefs_lru $drefs_lru $dmiss_lru $irefs_lip $drefs_lip $dmiss_lip $irefs_rand $drefs_rand $dmiss_rand $irefs_fifo $drefs_fifo $dmiss_fifo >> $output_file


done
cd - &> /dev/null
