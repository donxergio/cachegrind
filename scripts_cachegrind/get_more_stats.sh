#!/bin/bash

BENCHMARK_DIR=$1
benchmark_name=$2

#SIZES=(256 512 1024 2048 4096 8192 16384 32768 65536)
SIZES=(1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576 2097152 4194304 8388608)
WAYS=(1 2 4 8 16 32)
POLICIES=('lru' 'lip' 'random' 'fifo' 'bip0.015625' 'bip0.03125' 'bip0.0625')

cd $BENCHMARK_DIR
for p in ${POLICIES[@]}
do
	log_filename="stats_"$benchmark_name"_"$p".log"
	printf "%-10s %-13s %-13s %-13s %-13s %-13s %-13s %-13s %-13s %-13s %-13s %-13s %-13s\n" size miss1  miss2 miss4 miss8 miss16 miss32 cpi1 cpi2 cpi4 cpi8 cpi16 cpi32  > $log_filename
	for s in ${SIZES[@]}
	do
		for w in ${WAYS[@]}
		do
			filename[$w]=$(echo results_$benchmark_name\_$s\_$w\ways\_$p.log)
			cat ${filename[$w]} | grep "Cache set count is not a power of two." &> /dev/null
			if [ $? -eq 0 ]; then
				dmiss[$w]=0
			    	drefs[$w]=0
			    	irefs[$w]=0
			    	miss_ratio[$w]=0
			    	cpi[$w]=0
			else
				dmiss[$w]=$(cat ${filename[$w]} | grep "D1  misses" | awk '{print $4}' |  sed s/,//g)
			        drefs[$w]=$(cat ${filename[$w]} | grep "D   refs:" | awk '{print $4}' |  sed s/,//g)
			        irefs[$w]=$(cat ${filename[$w]} | grep "I   refs:" | awk '{print $4}' |  sed s/,//g)
		        	miss_ratio[$w]=$(python -c "print(\"%.3f\" % (100 * float(${dmiss[$w]}) / ${drefs[$w]}  ))")
		        	cpi[$w]=$(python -c "print(\"%.3f\" % (1000 * float(${dmiss[$w]}) / (${irefs[$w]}) ) )")
			fi
		done

		printf "%-10d %-13.5f %-13.5f %-13.5f %-13.5f %-13.5f %-13.5f %-13.5f %-13.5f %-13.5f %-13.5f %-13.5f %-13.5f\n" $s ${miss_ratio[1]} ${miss_ratio[2]} ${miss_ratio[4]} ${miss_ratio[8]} ${miss_ratio[16]} ${miss_ratio[32]} ${cpi[1]} ${cpi[2]} ${cpi[4]} ${cpi[8]} ${cpi[16]} ${cpi[32]} >> $log_filename	
	done

done
cd - &> /dev/null

