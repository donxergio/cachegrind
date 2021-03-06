#!/bin/bash

BENCHMARK_DIR=$1
benchmark_name=$2

SIZES=(256 512 1024 2048 4096 8192 16384 32768 65536)

cd $BENCHMARK_DIR
log_filename="stats_"$benchmark_name".log"
#echo -e "size\tmiss_ratio_lru\tmiss_ratio_lip\tmiss_ratio_rnd\tmiss_ratio_fif\tcpi_lru\t\tcpi_lip\t\tcpi_rnd\t\tcpi_fifo" > $log_filename
printf "%-10s %-15s %-15s %-15s %-15s %-15s %-15s %-15s %-15s\n" size miss_ratio_lru  miss_ratio_lip miss_ratio_rnd miss_ratio_fif cpi_lru cpi_lip cpi_rnd cpi_fifo  > $log_filename
for s in ${SIZES[@]}
do
	filename_lru=$(echo results_$benchmark_name\_$s\_lru.log)
        filename_lip=$(echo results_$benchmark_name\_$s\_lip.log)
        filename_random=$(echo results_$benchmark_name\_$s\_random.log)
        filename_fifo=$(echo results_$benchmark_name\_$s\_fifo.log)
        dmiss_lru=$(cat $filename_lru | grep "D1  misses" | awk '{print $4}' |  sed s/,//g)
        drefs_lru=$(cat $filename_lru | grep "D   refs:" | awk '{print $4}' |  sed s/,//g)
        irefs_lru=$(cat $filename_lru | grep "I   refs:" | awk '{print $4}' |  sed s/,//g)
        miss_ratio_lru=$(python -c "print(\"%.3f\" % (100 * float($dmiss_lru) / $drefs_lru  ))")
        cpi_lru=$(python -c "print(\"%.3f\" % (1000 * float($dmiss_lru) / ($irefs_lru) ) )")
	dmiss_lip=$(cat $filename_lip | grep "D1  misses" | awk '{print $4}' |  sed s/,//g)
        drefs_lip=$(cat $filename_lip | grep "D   refs:" | awk '{print $4}' |  sed s/,//g)
        irefs_lip=$(cat $filename_lip | grep "I   refs:" | awk '{print $4}' |  sed s/,//g)
        miss_ratio_lip=$(python -c "print(\"%.3f\" % (100 * float($dmiss_lip) / $drefs_lip  ))")
        cpi_lip=$(python -c "print(\"%.3f\" % (1000 * float($dmiss_lip) / ($irefs_lip) ) )")
	dmiss_rand=$(cat $filename_random | grep "D1  misses" | awk '{print $4}' |  sed s/,//g)
        drefs_rand=$(cat $filename_random | grep "D   refs:" | awk '{print $4}' |  sed s/,//g)
        irefs_rand=$(cat $filename_random | grep "I   refs:" | awk '{print $4}' |  sed s/,//g)
        miss_ratio_rand=$(python -c "print(\"%.3f\" % (100 * float($dmiss_rand) / $drefs_rand  ))")
        cpi_rand=$(python -c "print(\"%.3f\" % (1000 * float($dmiss_rand) / ($irefs_rand) ) )")
	dmiss_fifo=$(cat $filename_fifo | grep "D1  misses" | awk '{print $4}' |  sed s/,//g)
        drefs_fifo=$(cat $filename_fifo | grep "D   refs:" | awk '{print $4}' |  sed s/,//g)
        irefs_fifo=$(cat $filename_fifo | grep "I   refs:" | awk '{print $4}' |  sed s/,//g)
        miss_ratio_fifo=$(python -c "print(\"%.3f\" % (100 * float($dmiss_fifo) / $drefs_fifo  ))")
        cpi_fifo=$(python -c "print(\"%.3f\" % (1000 * float($dmiss_fifo) / ($irefs_fifo) ) )")

	printf "%-10d %-15.5f %-15.5f %-15.5f %-15.5f %-15.5f %-15.5f %-15.5f %-15.5f\n" $s $miss_ratio_lru $miss_ratio_lip $miss_ratio_rand $miss_ratio_fifo $cpi_lru $cpi_lip $cpi_rand $cpi_fifo  >> $log_filename

#	echo -e $s'\t'$miss_ratio_lru'\t\t\t'$miss_ratio_lip'\t\t\t\t'$miss_ratio_rand'\t\t'$miss_ratio_fifo'\t\t'$cpi_lru'\t\t'$cpi_lip'\t\t'$cpi_rand'\t\t'$cpi_fifo >> $log_filename
done
cd - &> /dev/null

