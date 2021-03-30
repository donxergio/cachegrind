#!/usr/bin/env python
from collections import defaultdict
from copy import deepcopy
from datetime import datetime
import sys
import os
import subprocess, shlex

home_dir     = "/home/tkloda/cortexsuite/"
valgrind_dir = "/home/tkloda/valgrind/bin/valgrind"
results_dir  =  home_dir + "/cachegrind_results_per_way/" 

benchmarks_list = []


#ways       = 4
#cache_line = 32

#ways       = 8
# now we run each benchmark for 1,2,4,8,16, and 32 ways
cache_line = 64


benchmarks_list_large_ws = [ "disparity-fullhd", "disparity-vga", "kmeans-large", "liblinear-tlarge", "liblinear-tsmall", "pca-large", "pca-medium", "pca-small", "rbm-large", "rbm-medium", "sift-fullhd", "sift-vga", "spc-large", "spc-medium", "sphinx-large", "stitch-fullhd", "stitch-vga", "svd3-large", "tracking-vga", "tracking-fullhd"  ]

benchmarks_list_cortex = [ "kmeans-small", "kmeans-medium", "kmeans-large", "spc-small", "spc-medium", "spc-large", "lda-small", "lda-medium", "lda-large", "liblinear-tlarge",  "liblinear-tmedium",  "liblinear-tsmall", "me-large",  "me-medium",  "me-small", "pca-large", "pca-medium", "pca-small", "rbm-large",  "rbm-medium",  "rbm-small", "sphinx-small", "sphinx-medium", "sphinx-large", "srr-large", "srr-medium",  "srr-small", "svd3-small", "svd3-medium", "svd3-large"  ]

benchmarks_list_vision = [ ]

benchmarks_dirs = defaultdict(str)
benchmarks_cmds = defaultdict(str)



kmeans_subdir            = "cortex/clustering/kmeans/"
spectral_subdir          = "cortex/clustering/spectral/"
lda_subdir               = "cortex/lda/"
liblinear_subdir         = "cortex/liblinear/"
motion_estimation_subdir = "cortex/motion-estimation/"
pca_subdir               = "cortex/pca/"
rbm_subdir               = "cortex/rbm/"
sphinx_subdir            = "cortex/sphinx/"
srr_subdir               = "cortex/srr/"
svd3_subdir              = "cortex/svd3/"

mser_subdir              = "vision/benchmarks/mser/data/"
sift_subdir              = "vision/benchmarks/sift/data/"
multi_ncut_subdir        = "vision/benchmarks/multi_ncut/data/"
svm_subdir               = "vision//benchmarks/svm/data/"
disparity_subdir         = "vision/benchmarks/disparity/data/"
stitch_subdir            = "vision/benchmarks/stitch/data/"
tracking_subdir          = "vision/benchmarks/tracking/data/"
localization_subdir      = "vision/benchmarks/localization/data/"
texture_synthesis_subdir = "vision/benchmarks/texture_synthesis/data/"



for data_size in [ "cif",  "fullhd", "qcif",  "sim",  "sim_fast",  "sqcif",  "test",  "vga" ]:
    benchmarks_dirs["mser-" + data_size]                    = home_dir + mser_subdir              + "/" + data_size + "/"
    benchmarks_dirs["disparity-" + data_size]               = home_dir + disparity_subdir         + "/" + data_size + "/"
    benchmarks_dirs["multi_ncut-" + data_size]              = home_dir + multi_ncut_subdir        + "/" + data_size + "/"
    benchmarks_dirs["sift-" + data_size]                    = home_dir + sift_subdir              + "/" + data_size + "/"
    benchmarks_dirs["stitch-" + data_size]                  = home_dir + stitch_subdir            + "/" + data_size + "/"
    benchmarks_dirs["texture_synthesis-" + data_size]       = home_dir + texture_synthesis_subdir + "/" + data_size + "/"
    benchmarks_dirs["tracking-" + data_size]                = home_dir + tracking_subdir          + "/" + data_size + "/"

    benchmarks_cmds["mser-" + data_size]                    = "./mser ."
    benchmarks_cmds["disparity-" + data_size]               = "./disparity ."
    benchmarks_cmds["multi_ncut-" + data_size]              = "./multi_ncut ."
    benchmarks_cmds["sift-" + data_size]                    = "./sift ."
    benchmarks_cmds["stitch-" + data_size]                  = "./stitch ."
    benchmarks_cmds["texture_synthesis-" + data_size]       = "./texture_synthesis ."
    benchmarks_cmds["tracking-" + data_size]                = "./tracking ."

    benchmarks_list_vision.append("mser-" + data_size)
    benchmarks_list_vision.append("disparity-" + data_size)
    benchmarks_list_vision.append("multi_ncut-" + data_size)
    benchmarks_list_vision.append("sift-" + data_size)
    benchmarks_list_vision.append("stitch-" + data_size)
    benchmarks_list_vision.append("texture_synthesis-" + data_size)
    benchmarks_list_vision.append("tracking-" + data_size)


for data_size in [ "cif",  "qcif",  "sim",  "sim_fast",  "sqcif",  "test",  "vga" ]:
    benchmarks_dirs["localization-" + data_size]      = home_dir + localization_subdir + "/" + data_size + "/"
    benchmarks_cmds["localization-" + data_size]      = "./localization ."
    benchmarks_list_vision.append("localization-" + data_size)


for data_size in [ "cif",  "qcif",  "sim",  "sim_fast",  "sqcif",  "test" ]:
    benchmarks_dirs["svm-" + data_size]  = home_dir + svm_subdir + "/" + data_size + "/"
    benchmarks_cmds["svm-" + data_size]  = "./svm ."
    benchmarks_list_vision.append("svm-" + data_size)




benchmarks_dirs["kmeans-small"]      = home_dir + kmeans_subdir
benchmarks_dirs["kmeans-medium"]     = home_dir + kmeans_subdir
benchmarks_dirs["kmeans-large"]      = home_dir + kmeans_subdir
benchmarks_dirs["spc-small"]         = home_dir + spectral_subdir
benchmarks_dirs["spc-medium"]        = home_dir + spectral_subdir
benchmarks_dirs["spc-large"]         = home_dir + spectral_subdir
benchmarks_dirs["lda-small"]         = home_dir + lda_subdir
benchmarks_dirs["lda-medium"]        = home_dir + lda_subdir
benchmarks_dirs["lda-large"]         = home_dir + lda_subdir
benchmarks_dirs["liblinear-tlarge"]  = home_dir + liblinear_subdir
benchmarks_dirs["liblinear-tmedium"] = home_dir + liblinear_subdir
benchmarks_dirs["liblinear-tsmall"]  = home_dir + liblinear_subdir
benchmarks_dirs["me-large"]          = home_dir + motion_estimation_subdir
benchmarks_dirs["me-medium"]         = home_dir + motion_estimation_subdir
benchmarks_dirs["me-small"]          = home_dir + motion_estimation_subdir
benchmarks_dirs["pca-large"]         = home_dir + pca_subdir
benchmarks_dirs["pca-medium"]        = home_dir + pca_subdir
benchmarks_dirs["pca-small"]         = home_dir + pca_subdir
benchmarks_dirs["rbm-large"]         = home_dir + rbm_subdir
benchmarks_dirs["rbm-medium"]        = home_dir + rbm_subdir
benchmarks_dirs["rbm-small"]         = home_dir + rbm_subdir
benchmarks_dirs["sphinx-small"]      = home_dir + sphinx_subdir
benchmarks_dirs["sphinx-medium"]     = home_dir + sphinx_subdir
benchmarks_dirs["sphinx-large"]      = home_dir + sphinx_subdir
benchmarks_dirs["srr-small"]         = home_dir + srr_subdir
benchmarks_dirs["srr-medium"]        = home_dir + srr_subdir
benchmarks_dirs["srr-large"]         = home_dir + srr_subdir
benchmarks_dirs["svd3-small"]        = home_dir + svd3_subdir
benchmarks_dirs["svd3-medium"]       = home_dir + svd3_subdir
benchmarks_dirs["svd3-large"]        = home_dir + svd3_subdir




benchmarks_cmds["kmeans-small"]      = "./kmeans-small ../datasets/yeast 1484 8 10"
benchmarks_cmds["kmeans-medium"]     = "./kmeans-medium ../datasets/finland 13467 2 15"
benchmarks_cmds["kmeans-large"]      = "./kmeans-large ../datasets/MINST 10000 748 10"
benchmarks_cmds["spc-small"]         = "./spc-small ../datasets/R15 600 2 15 0.707 1 2"
benchmarks_cmds["spc-medium"]        = "./spc-medium ../datasets/Aggregation 788 2 7 0.707 1"
benchmarks_cmds["spc-large"]         = "./spc-small ../datasets/R15 600 2 15 0.707 1 2"
benchmarks_cmds["lda-small"]         = "./lda-small est .1 3 settings.txt small/small_data.dat random small/result"
benchmarks_cmds["lda-medium"]        = "./lda-medium est .1 3 settings.txt medium/medium_data.dat random medium/results"
benchmarks_cmds["lda-large"]         = "./lda-large est .1 3 settings.txt large/large_data.dat random large/results"
benchmarks_cmds["liblinear-tlarge"]  = "./liblinear-tlarge data/100B/kdda"
benchmarks_cmds["liblinear-tmedium"] = "./liblinear-tmedium data/10B/epsilon"
benchmarks_cmds["liblinear-tsmall"]  = "./liblinear-tsmall data/100M/crime_scale"
benchmarks_cmds["me-large"]          = "./me-large"
benchmarks_cmds["me-medium"]         = "./me-medium"
benchmarks_cmds["me-small"]          = "./me-small"
benchmarks_cmds["pca-large"]         = "./pca-large large.data 5000 1059 R"
benchmarks_cmds["pca-medium"]        = "./pca-medium medium.data 722 800 R"
benchmarks_cmds["pca-small"]         = "./pca-small small.data 1593 256 R"
benchmarks_cmds["rbm-large"]         = "./rbm-large"
benchmarks_cmds["rbm-medium"]        = "./rbm-medium"
benchmarks_cmds["rbm-small"]         = "./rbm-small"
benchmarks_cmds["sphinx-small"]      = "./sphinx small/audio.raw language_model/turtle"
benchmarks_cmds["sphinx-medium"]     = "./sphinx medium/audio.raw language_model/HUB4/"
benchmarks_cmds["sphinx-large"]      = "./sphinx large/audio.raw language_model/HUB4/"
benchmarks_cmds["srr-small"]         = "./srr-small"
benchmarks_cmds["srr-medium"]        = "./srr-medium"
benchmarks_cmds["srr-large"]         = "./srr-large"
benchmarks_cmds["svd3-small"]        = "./a.out small.txt"
benchmarks_cmds["svd3-medium"]       = "./a.out med.txt"
benchmarks_cmds["svd3-large"]        = "./a.out large.txt"




def main():
    global benchmarks_list
    
    if len(sys.argv) != 3:
        print "Specify [run|stats|get] [cortex|vision|all|largews]"
        return 1
    
    if sys.argv[2] == "cortex":
        benchmarks_list = benchmarks_list_cortex
    elif sys.argv[2] == "vision":
        benchmarks_list = benchmarks_list_vision
    elif sys.argv[2] == "all":
        benchmarks_list = benchmarks_list_cortex + benchmarks_list_vision
    elif sys.argv[2] == "largews":
        benchmarks_list = benchmarks_list_large_ws
    else:
        print "Wrong scope [cortex|vision|all]"
        return 1
    
    if sys.argv[1] == "run":
        run_all()
    elif sys.argv[1] == "stats":
        stats_all()
    elif sys.argv[1] == "get":
        get_all()
    else:
        print "Wrong action [run|stats|get]"
        return 1

    return 0


def valgrind_call(benchmark_name,cache_size, ways, line, policy, policy_opt=""):

    log_name = "results_" + benchmark_name + "_" + str(cache_size) + "_" + str(ways) + "ways_" + policy + str(policy_opt) + ".log"
    benchmark_cmd = benchmarks_cmds[benchmark_name]

    if policy=="bip":
        #valgrind_cmd = "%s --tool=cachegrind --I1=%d,%d,%d --D1=%d,%d,%d --trace-children=yes --cache-policy=bip --bip-throttle=%s %s &> %s  &" % \
        valgrind_cmd = "%s --tool=cachegrind --I1=%d,%d,%d --D1=%d,%d,%d --trace-children=yes --cache-policy=bip --bip-throttle=%s %s 2> %s 1> /dev/null &" % \
            (valgrind_dir,cache_size,ways,line,cache_size,ways,line,policy_opt,benchmark_cmd,log_name)
    else:
        valgrind_cmd = "%s --tool=cachegrind --I1=%d,%d,%d --D1=%d,%d,%d --trace-children=yes --cache-policy=%s  %s 2> %s 1> /dev/null &" % \
            (valgrind_dir,cache_size,ways,line,cache_size,ways,line,policy,benchmark_cmd,log_name)

    os.system(valgrind_cmd)


def run_all():
    kB=1024
    MB=1024*1024
    for benchmark in benchmarks_list:
        print benchmark
        os.chdir(benchmarks_dirs[benchmark])
        for cache_policy in ["lru", "lip", "random", "fifo"]:
            for cache_size in [1*kB,2*kB,4*kB,8*kB,16*kB,32*kB,64*kB,128*kB,256*kB,512*kB,1*MB,2*MB,4*MB,8*MB]:
                for ways in [1,2,4,8,16,32]:
                    valgrind_call(benchmark,cache_size,ways,cache_line,cache_policy)
        for bip in [1.0/16, 1.0/32, 1.0/64]:
            for cache_size in [1*kB,2*kB,4*kB,8*kB,16*kB,32*kB,64*kB,128*kB,256*kB,512*kB,1*MB,2*MB,4*MB,8*MB]:
                for ways in [1,2,4,8,16,32]:
                    valgrind_call(benchmark,cache_size,ways,cache_line,"bip",bip)


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

