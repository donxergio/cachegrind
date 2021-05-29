#!/usr/bin/env python
from collections import defaultdict
from copy import deepcopy
from datetime import datetime
import sys
import os
import subprocess, shlex

home_dir     = "/home/Bruna/valgrind_tum/dis_stressmark/"
valgrind_dir = "/home/Bruna/valgrind_tum/valgrind/src/bin/valgrind"
results_dir  =  home_dir + "/cachegrind_results/" 

benchmarks_list = []


#ways       = 4
#cache_line = 32

#ways       = 8
# now we run each benchmark for 1,2,4,8,16, and 32 ways
cache_line = 64

benchmarks_list = [ "update_small"]
#benchmarks_list_large_ws = [ "disparity-fullhd", "disparity-vga", "kmeans-large", "liblinear-tlarge", "liblinear-tsmall", "pca-large", "pca-medium", "pca-small", "rbm-large", "rbm-medium", "sift-fullhd", "sift-vga", "spc-large", "spc-medium", "sphinx-large", "stitch-fullhd", "stitch-vga", "svd3-large", "tracking-vga", "tracking-fullhd"  ]

#benchmarks_list_cortex = [ "kmeans-small", "kmeans-medium", "kmeans-large", "spc-small", "spc-medium", "spc-large", "lda-small", "lda-medium", "lda-large", "liblinear-tlarge",  "liblinear-tmedium",  "liblinear-tsmall", "me-large",  "me-medium",  "me-small", "pca-large", "pca-medium", "pca-small", "rbm-large",  "rbm-medium",  "rbm-small", "sphinx-small", "sphinx-medium", "sphinx-large", "srr-large", "srr-medium",  "srr-small", "svd3-small", "svd3-medium", "svd3-large"  ]

#benchmarks_list_vision = [ ]

benchmarks_dirs = defaultdict(str)
benchmarks_cmds = defaultdict(str)



update_subdir = "update/"


#benchmarks_dirs["update_large"]        = home_dir + update_subdir
benchmarks_dirs["update_small"]        = home_dir + update_subdir

#benchmarks_cmds["update_large"] = "./update_large"
benchmarks_cmds["update_small"] = "./update_small"


def main():
    if len(sys.argv) != 2:
        print ("Specify action [run|stats|get]")
        return 1
    if sys.argv[1] == "run":
        run_all()
    elif sys.argv[1] == "stats":
        stats_all()
    elif sys.argv[1] == "get":
        get_all()
    else:
        print ("Wrong action")
        return 1
    return 0


def valgrind_call(benchmark_name,cache_size, ways, line, policy, policy_opt=""):

    log_name = "results_" + benchmark_name + "_" + str(cache_size) + "_" + str(ways) + "ways_" + policy + str(policy_opt) + ".log"
    benchmark_cmd = benchmarks_cmds[benchmark_name]

    if policy=="bip":
        #valgrind_cmd = "%s --tool=cachegrind --I1=%d,%d,%d --D1=%d,%d,%d --trace-children=yes --cache-policy=bip --bip-throttle=%s %s &> %s  &" % \
        valgrind_cmd = "%s --tool=cachegrind --I1=%d,%d,%d --D1=%d,%d,%d --trace-children=yes --cache-policy=bip --bip-throttle=%s %s 2> %s 1> /dev/null &" % \
            (valgrind_dir,cache_size,ways,line,cache_size,ways,line,policy_opt,benchmark_cmd,log_name)
    elif policy=="dip":
        #valgrind_cmd = "%s --tool=cachegrind --I1=%d,%d,%d --D1=%d,%d,%d --trace-children=yes --cache-policy=dip --bip-throttle=%s %s &> %s  &" % \
        valgrind_cmd = "%s --tool=cachegrind --I1=%d,%d,%d --D1=%d,%d,%d --trace-children=yes --cache-policy=dip --bip-throttle=%s %s 2> %s 1> /dev/null &" % \
            (valgrind_dir,cache_size,ways,line,cache_size,ways,line,policy_opt,benchmark_cmd,log_name)
    else:
        valgrind_cmd = "%s --tool=cachegrind --I1=%d,%d,%d --D1=%d,%d,%d --trace-children=yes --cache-policy=%s  %s 2> %s 1> /dev/null &" % \
            (valgrind_dir,cache_size,ways,line,cache_size,ways,line,policy,benchmark_cmd,log_name)

    os.system(valgrind_cmd)


def run_all():
    kB=1024
    MB=1024*1024
    for benchmark in benchmarks_list:
        print (benchmark)
        os.chdir(benchmarks_dirs[benchmark])
        #for cache_policy in ["lru", "lip", "random", "fifo"]:
        #for cache_policy in ["lru"]:
        #    for cache_size in [1*kB,2*kB,4*kB,8*kB,16*kB,32*kB,64*kB,128*kB,256*kB,512*kB,1*MB,2*MB,4*MB,8*MB]:
         #       for ways in [1,2,4,8,16,32]:
                #for ways in [1]:
          #          valgrind_call(benchmark,cache_size,ways,cache_line,cache_policy)
        #for bip in [1.0/16, 1.0/32, 1.0/64]:
        #for bip in [1.0/64]:
         #   for cache_size in [1*kB,2*kB,4*kB,8*kB,16*kB,32*kB,64*kB,128*kB,256*kB,512*kB,1*MB,2*MB,4*MB,8*MB]:
          #      for ways in [1,2,4,8,16,32]:
           #         valgrind_call(benchmark,cache_size,ways,cache_line,"bip",bip)
        for cache_policy in ["dip"]:
            for bip in [1.0/16]:
                for cache_size in [1*kB,2*kB,4*kB,8*kB,16*kB,32*kB,64*kB,128*kB,256*kB,512*kB,1*MB,2*MB,4*MB,8*MB]:
                    for ways in [1,2,4,8,16,32]:
                        valgrind_call(benchmark,cache_size,ways,cache_line,"dip",bip)


def stats_all():
    for benchmark in benchmarks_list:
       os.system("./get_more_stats.sh " + benchmarks_dirs[benchmark] + " " + benchmark + " &")


def get_all():
    os.chdir(home_dir)
    os.system("mkdir -p " + results_dir )
    for benchmark in benchmarks_list:
        for  cache_policy in ["lru", "lip", "random", "fifo", "bip0.0625", "bip0.03125", "bip0.015625" ]:
            stats_filename = benchmarks_dirs[benchmark] + "stats_" + benchmark + "_" + cache_policy + ".log"
            copy_cmd       = "cp " + stats_filename + " " + results_dir
            os.system(copy_cmd)



if __name__ == "__main__":
    sys.exit(main())

