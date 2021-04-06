#!/usr/bin/python3

from sys import argv
import argparse
import csv
import pandas as pd

class Input_Files:
    def __init__(self, benchmark_name, filename=''):
        self.benchmark_name = benchmark_name
        self.filename = filename
        self.ways = ""


input_files = []
input_files.append(Input_Files("adpcm_large_enc"))
input_files.append(Input_Files("adpcm_small_dec"))
input_files.append(Input_Files("basicmath_large"))
input_files.append(Input_Files("basicmath_small"))
input_files.append(Input_Files("bf_large_dec"))
input_files.append(Input_Files("bf_small_enc"))
input_files.append(Input_Files("bitcount_large"))
input_files.append(Input_Files("bitcount_small"))
input_files.append(Input_Files("crc32"))
input_files.append(Input_Files("dijkstra_large"))
input_files.append(Input_Files("dijkstra_small"))
input_files.append(Input_Files("fft_large"))
input_files.append(Input_Files("fft_small"))
input_files.append(Input_Files("fft_small_inv"))
input_files.append(Input_Files("gsm_large_dec"))
input_files.append(Input_Files("gsm_large_enc"))
input_files.append(Input_Files("gsm_small_dec"))
input_files.append(Input_Files("gsm_small_enc"))
input_files.append(Input_Files("jpeg_large_ppm"))
input_files.append(Input_Files("jpeg_large_progressive"))
input_files.append(Input_Files("jpeg_small_ppm"))
input_files.append(Input_Files("jpeg_small_progressive"))
input_files.append(Input_Files("patricia_large"))
input_files.append(Input_Files("patricia_small"))
input_files.append(Input_Files("qsort_large"))
input_files.append(Input_Files("qsort_small"))
input_files.append(Input_Files("rijndael_large_dec"))
input_files.append(Input_Files("rijndael_large_enc"))
input_files.append(Input_Files("rijndael_small_dec"))
input_files.append(Input_Files("rijndael_small_enc"))
input_files.append(Input_Files("sha_large"))
input_files.append(Input_Files("sha_small"))
input_files.append(Input_Files("stringsearch_large"))
input_files.append(Input_Files("stringsearch_small"))
input_files.append(Input_Files("susan_large"))
input_files.append(Input_Files("susan_large_corner"))
input_files.append(Input_Files("susan_large_edge"))
input_files.append(Input_Files("susan_large_smooth"))
input_files.append(Input_Files("susan_small"))
input_files.append(Input_Files("susan_small_corner"))
input_files.append(Input_Files("susan_small_edge"))
input_files.append(Input_Files("susan_small_smooth"))


directory="/home/giovani/ufsc/riscv/valgrind/mibench_cachegrind_logs/"

class Cache_Parameters:
    def __init__(self, proc_name, cpi, d1_hit_penalty, d1_miss_penalty):
        self.proc_name = proc_name
        self.cpi = cpi
        self.d1_hit_penalty = d1_hit_penalty
        self.d1_miss_penalty = d1_miss_penalty

cache_parameters = []
cache_parameters.append(Cache_Parameters("A53", 0.5, 19.692, 181.416)) #parameters for the ARM A53 processor measured by https://ospert18.ittc.ku.edu/proceedings-ospert2018.pdf#page=56
cache_parameters.append(Cache_Parameters("i7", 0.25, 10, 135)) #parameters for the Intel i7 processor
cache_parameters.append(Cache_Parameters("A8", 0.5, 11, 60)) #parameters for the ARM Cortex A8  processor
cache_parameters.append(Cache_Parameters("x86", 0.5, 3, 44)) #parameters for the x86  processor in https://www.eecg.utoronto.ca/~jayar/pubs/wong/wongtrets16.pdf
cache_parameters.append(Cache_Parameters("bip_paper", 0.25, 7, 270)) #parameters used in BIP/LIP paper
#TODO: add the numbers we will get for RISC-V


#wcet = (I refs) x number of instructions per cycle + (D1 misses) x cache miss penalty + (D refs - D1 misses) x cache hit penalty
def calculate_wcet(number_of_instructions, data_references, d1_misses, cache_params):
    return (number_of_instructions * cache_params.cpi) + (d1_misses * cache_params.d1_miss_penalty) + ((data_references - d1_misses) * cache_params.d1_hit_penalty)

def l1_cache_analysis(input_file_info, cache_params):

    data = pd.read_csv(input_file_info.filename, sep=',')

    output_file = input_file_info.benchmark_name + "_" + cache_params.proc_name + ".csv"

    file = open(directory+output_file, "w")

    file.write("SIZE,LRU,LIP,RANDOM,FIFO,BIP0.015625,BIP0.03125,BIP0.0625\n")
    for index, row in data.iterrows():
        wcet_lru = calculate_wcet(row['LRU-I'], row['LRU-D'], row['LRU-M'], cache_params)

        wcet_lip = calculate_wcet(row['LIP-I'], row['LIP-D'], row['LIP-M'], cache_params)

        wcet_rand = calculate_wcet(row['RANDOM-I'], row['RANDOM-D'], row['RANDOM-M'], cache_params)

        wcet_fifo = calculate_wcet(row['FIFO-I'], row['FIFO-D'], row['FIFO-M'], cache_params)

        wcet_bip0015625 = calculate_wcet(row['BIP0.015625-I'], row['BIP0.015625-D'], row['BIP0.015625-M'], cache_params)
        
        wcet_bip003125 = calculate_wcet(row['BIP0.03125-I'], row['BIP0.03125-D'], row['BIP0.03125-M'], cache_params)
        
        wcet_bip00625 = calculate_wcet(row['BIP0.0625-I'], row['BIP0.0625-D'], row['BIP0.0625-M'], cache_params)

        #print(str(row['SIZE'])+","+str(wcet_lru)+","+str(wcet_lip)+","+str(wcet_rand)+","+str(wcet_fifo))
        file.write(str(row['SIZE'])+","+str(wcet_lru)+","+str(wcet_lip)+","+str(wcet_rand)+","+str(wcet_fifo)+","+str(wcet_bip0015625)+","+str(wcet_bip003125)+","+str(wcet_bip00625)+"\n")

if __name__ == '__main__':
    
    #ways = ["1ways", "2ways", "4ways", "8ways", "16ways", "32ways"]
    for f in input_files:
        #for w in ways:
        f.filename = directory+f.benchmark_name+"_filtered.csv"
        #f.ways = w
        for c in cache_parameters:
            l1_cache_analysis(f, c)