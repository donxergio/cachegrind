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
input_files.append(Input_Files("kmeans-large")) #kmeans-large benchmark
input_files.append(Input_Files("kmeans-medium")) 
input_files.append(Input_Files("kmeans-small"))

input_files.append(Input_Files("disparity-cif"))
input_files.append(Input_Files("disparity-fullhd"))
input_files.append(Input_Files("disparity-qcif"))
input_files.append(Input_Files("disparity-vga"))

input_files.append(Input_Files("lda-large"))
input_files.append(Input_Files("lda-medium"))
input_files.append(Input_Files("lda-small"))

input_files.append(Input_Files("me-large"))
input_files.append(Input_Files("me-medium"))
input_files.append(Input_Files("me-small"))

input_files.append(Input_Files("mser-cif"))
input_files.append(Input_Files("mser-fullhd"))
input_files.append(Input_Files("mser-qcif"))

input_files.append(Input_Files("multi_ncut-cif"))
input_files.append(Input_Files("multi_ncut-fullhd"))
input_files.append(Input_Files("multi_ncut-qcif"))

input_files.append(Input_Files("pca-large")) #pca-large benchmark
input_files.append(Input_Files("pca-medium"))
input_files.append(Input_Files("pca-small"))

input_files.append(Input_Files("rbm-large"))
input_files.append(Input_Files("rbm-medium"))
input_files.append(Input_Files("rbm-small"))

input_files.append(Input_Files("sift-cif"))
input_files.append(Input_Files("sift-fullhd"))
input_files.append(Input_Files("sift-qcif"))
input_files.append(Input_Files("sift-vga"))

input_files.append(Input_Files("spc-large"))
input_files.append(Input_Files("spc-medium"))
input_files.append(Input_Files("spc-small"))

input_files.append(Input_Files("sphinx-large"))
input_files.append(Input_Files("sphinx-medium"))
input_files.append(Input_Files("sphinx-small"))

input_files.append(Input_Files("stitch-cif"))
input_files.append(Input_Files("stitch-fullhd"))
input_files.append(Input_Files("stitch-vga"))

input_files.append(Input_Files("liblinear-tlarge"))
input_files.append(Input_Files("liblinear-tmedium"))
input_files.append(Input_Files("liblinear-tsmall"))

input_files.append(Input_Files("svd3-large"))
input_files.append(Input_Files("svd3-medium"))
input_files.append(Input_Files("svd3-small"))

input_files.append(Input_Files("srr-large"))
input_files.append(Input_Files("srr-medium"))
input_files.append(Input_Files("srr-small"))

input_files.append(Input_Files("texture_synthesis-cif"))
input_files.append(Input_Files("texture_synthesis-fullhd"))

input_files.append(Input_Files("tracking-cif"))
input_files.append(Input_Files("tracking-fullhd"))
input_files.append(Input_Files("tracking-vga"))


directory="/home/giovani/ufsc/riscv/valgrind/cortex_cachegrind_logs/"

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

    output_file = input_file_info.benchmark_name + "_" + input_file_info.ways + "_" + cache_params.proc_name + ".csv"

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

        #TODO: ADD BIP HERE

        #print(str(row['SIZE'])+","+str(wcet_lru)+","+str(wcet_lip)+","+str(wcet_rand)+","+str(wcet_fifo))
        file.write(str(row['SIZE'])+","+str(wcet_lru)+","+str(wcet_lip)+","+str(wcet_rand)+","+str(wcet_fifo)+","+str(wcet_bip0015625)+","+str(wcet_bip003125)+","+str(wcet_bip00625)+"\n")

if __name__ == '__main__':
    
    ways = ["2ways", "4ways", "8ways", "16ways", "32ways"]
    for f in input_files:
        for w in ways:
            f.filename = directory+f.benchmark_name+"_"+w+"_filtered.csv"
            f.ways = w
            for c in cache_parameters:
                l1_cache_analysis(f, c)