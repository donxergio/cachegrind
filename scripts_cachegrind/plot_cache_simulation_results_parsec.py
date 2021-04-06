#!/usr/bin/python3
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from matplotlib.backends.backend_pdf import PdfPages

class Input_Files:
    def __init__(self, benchmark_name, filename=''):
        self.benchmark_name = benchmark_name
        self.filename = filename
        self.ways = ""

input_files = []
input_files.append(Input_Files("canneal-large"))
input_files.append(Input_Files("ferret-large"))
input_files.append(Input_Files("raytrace-large"))
input_files.append(Input_Files("raytrace-medium"))
input_files.append(Input_Files("streamcluster-large"))
input_files.append(Input_Files("swaptions-large"))
input_files.append(Input_Files("vips-large"))
input_files.append(Input_Files("vips-medium"))
input_files.append(Input_Files("vips-small"))

directory="/home/giovani/ufsc/riscv/valgrind/parsec_cachegrind_logs/results/"

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


def plot_graphs(input_file_info, cache_params, pp):
    data = pd.read_csv(input_file_info.filename)

    for i in range(0,14):
        lru = data.loc[i,'LRU']
        data.loc[i,'LIP'] = float(data.loc[i,'LIP']) / lru 
        data.loc[i,'RANDOM'] = float(data.loc[i,'RANDOM']) / lru
        data.loc[i,'FIFO'] = float(data.loc[i,'FIFO']) / lru
        #data.loc[i,'BIP0.015625'] = float(data.loc[i,'BIP0.015625']) / lru 
        #data.loc[i,'BIP0.03125'] = float(data.loc[i,'BIP0.03125']) / lru
        #data.loc[i,'BIP0.0625'] = float(data.loc[i,'BIP0.0625']) / lru
        data.loc[i,'LRU'] = 1.0
        
    data.plot.bar(0, [1, 2, 3, 4, 5, 6, 7])
    plt.title(input_file_info.benchmark_name + " " + f.ways + " " + cache_params.proc_name)
    plt.xlabel("Cache Partition Size (in bytes)")
    plt.ylabel("Execution Time (in cycles) - Normalized according to LRU")
    plt.ylim([0.8, 1.5])
    plt.legend(ncol=3)
    plt.tight_layout()
    #plt.show()
    plt.savefig(pp, format='pdf')
    plt.close()

if __name__ == '__main__':

    #ways = ["1ways", "2ways", "4ways", "8ways", "16ways", "32ways"]
    old = ''
    for f in input_files:
        name = f.benchmark_name.partition("-")[0]
        if old == '':
            pp = PdfPages(directory + name + "_normalized.pdf")
        else:
            if name != old:
                pp.close()
                pp = PdfPages(directory + name + "_normalized.pdf")

        print("Generating graph for " + f.benchmark_name)
       
        for c in cache_parameters:
            f.filename = directory+f.benchmark_name+"_"+c.proc_name+".csv"
            #print(f.filename)
            plot_graphs(f, c, pp)
        old = name
    pp.close()