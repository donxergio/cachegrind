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

input_files.append(Input_Files("liblinear-tlarge"))
input_files.append(Input_Files("liblinear-tmedium"))
input_files.append(Input_Files("liblinear-tsmall"))

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

#input_files = []
#input_files.append("svd3-large_4ways_bip_paper.csv")

directory="/home/giovani/ufsc/riscv/valgrind/cortex_cachegrind_logs/results/"

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

def test_difference(data, i, size, input_file_info, cache_params):
    if data.loc[i,'LIP'] != 0.0 and data.loc[i,'LIP'] < data.loc[i,'LRU'] and (1.0 - data.loc[i,'LIP']) > 0.15:
        print(input_file_info.benchmark_name + "\t" + str(size) + "\t" + input_file_info.ways  + "\t" + "\tLIP\t" + "{:.4f}%\t".format(round((1.0-data.loc[i,'LIP'])*100.0)) + cache_params.proc_name)

    if data.loc[i,'RANDOM'] != 0.0 and data.loc[i,'RANDOM'] < data.loc[i,'LRU'] and (1.0 - data.loc[i,'RANDOM']) > 0.15:
        print(input_file_info.benchmark_name + "\t" + str(size) + "\t" + input_file_info.ways  + "\t" + "RANDOM\t" + "{:.4f}%\t".format(round((1.0-data.loc[i,'RANDOM'])*100.0)) + cache_params.proc_name)

    if data.loc[i,'FIFO'] != 0.0 and data.loc[i,'FIFO'] < data.loc[i,'LRU'] and (1.0 - data.loc[i,'FIFO']) > 0.15:
        print(input_file_info.benchmark_name + "\t" + str(size) + "\t" + input_file_info.ways  + "\t" + "FIFO\t" + "{:.4f}%\t".format(round((1.0-data.loc[i,'FIFO'])*100.0)) + cache_params.proc_name)

    if data.loc[i,'BIP0.015625'] != 0.0 and data.loc[i,'BIP0.015625'] < data.loc[i,'LRU'] and (1.0 - data.loc[i,'BIP0.015625']) > 0.15:
        print(input_file_info.benchmark_name + "\t" + str(size) + "\t" + input_file_info.ways  + "\t" + "BIP0.015625\t" + "{:.4f}%\t".format(round((1.0-data.loc[i,'BIP0.015625'])*100.0)) + cache_params.proc_name)

    if data.loc[i,'BIP0.03125'] != 0.0 and data.loc[i,'BIP0.03125'] < data.loc[i,'LRU'] and (1.0 - data.loc[i,'BIP0.03125']) > 0.15:
        print(input_file_info.benchmark_name + "\t" + str(size) + "\t" + input_file_info.ways  + "\t" + "BIP0.03125\t" + "{:.4f}%\t".format(round((1.0-data.loc[i,'BIP0.03125'])*100.0)) + cache_params.proc_name)

    if data.loc[i,'BIP0.0625'] != 0.0 and data.loc[i,'BIP0.0625'] < data.loc[i,'LRU'] and (1.0 - data.loc[i,'BIP0.0625']) > 0.15:
        print(input_file_info.benchmark_name + "\t" + str(size) + "\t" + input_file_info.ways  + "\t" + "BIP0.0625\t" + "{:.4f}%\t".format(round((1.0-data.loc[i,'BIP0.0625'])*100.0))+ cache_params.proc_name)

def plot_graphs(input_file_info, cache_params, pp):
    data = pd.read_csv(input_file_info.filename)

    for i in range(0,14):
        lru = data.loc[i,'LRU']
        data.loc[i,'LIP'] = float(data.loc[i,'LIP']) / lru 
        data.loc[i,'RANDOM'] = float(data.loc[i,'RANDOM']) / lru
        data.loc[i,'FIFO'] = float(data.loc[i,'FIFO']) / lru
        data.loc[i,'BIP0.015625'] = float(data.loc[i,'BIP0.015625']) / lru 
        data.loc[i,'BIP0.03125'] = float(data.loc[i,'BIP0.03125']) / lru
        data.loc[i,'BIP0.0625'] = float(data.loc[i,'BIP0.0625']) / lru
        data.loc[i,'LRU'] = 1.0

        #if(input_file_info.ways != "1ways"):
        #    test_difference(data, i, data.loc[i,'SIZE'], input_file_info, cache_params)

        
    data.plot.bar(0, [1, 2, 3, 4, 5, 6, 7])
    plt.title(input_file_info.benchmark_name + " " + f.ways + " " + cache_params.proc_name)
    plt.xlabel("Cache Partition Size (in bytes)")
    plt.ylabel("Execution Time (in cycles) - Normalized according to LRU")
    plt.ylim([0.7, 1.5])
    plt.legend(ncol=3)
    plt.tight_layout()
    #plt.show()
    plt.savefig(pp, format='pdf')
    plt.close()
    

if __name__ == '__main__':

    ways = ["2ways", "4ways", "8ways", "16ways", "32ways"]
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
        for w in ways:
            f.ways = w
            for c in cache_parameters:
                f.filename = directory+f.benchmark_name+"_"+w+"_"+c.proc_name+".csv"
                #print(f.filename)
                plot_graphs(f, c, pp)
        old = name
    pp.close()