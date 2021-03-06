#!/usr/bin/env python
from collections import defaultdict
from copy import deepcopy
from datetime import datetime
import sys
import os
import subprocess, shlex

home_dir     = "/home/tkloda/mibench/"
valgrind_dir = "/home/tkloda/valgrind/bin/valgrind"
results_dir  = "cachegrind_results" 


ways       = 4
cache_line = 32

benchmarks_list = [ "basicmath_large", "basicmath_small", "bitcount_small", "bitcount_large", "qsort_small", "qsort_large", "susan_small_edge", "susan_large_edge", "susan_small_corner", "susan_large_corner","susan_small_smooth", "susan_large_smooth", "jpeg_small_progressive", "jpeg_small_ppm", "jpeg_large_progressive", "jpeg_large_ppm", "stringsearch_small", "stringsearch_large", "dijkstra_large", "dijkstra_small", "patricia_large", "patricia_small", "bf_small_enc", "bf_small_dec", "bf_large_enc", "bf_large_dec", "sha_small", "sha_large", "rijndael_small_enc", "rijndael_small_dec", "rijndael_large_enc", "rijndael_large_dec", "gsm_small_enc", "gsm_large_enc", "gsm_small_dec", "gsm_large_dec", "fft_small", "fft_small_inv", "fft_large", "fft_large_inv", "adpcm_small_enc", "adpcm_large_enc", "adpcm_large_enc", "adpcm_small_dec", "crc32" ]


benchmarks_dirs = defaultdict(str)
benchmarks_cmds = defaultdict(str)

automotive_subdir = "automotive/"
consumer_subdir   = "consumer/"
office_subdir     = "office/"
network_subdir    = "network/"
security_subdir   = "security/"
telecomm_subdir   = "telecomm/"


benchmarks_dirs["basicmath_large"]        = home_dir + automotive_subdir + "basicmath/"
benchmarks_dirs["basicmath_small"]        = home_dir + automotive_subdir + "basicmath/"
benchmarks_dirs["bitcount_small"]         = home_dir + automotive_subdir + "bitcount/"
benchmarks_dirs["bitcount_large"]         = home_dir + automotive_subdir + "bitcount/"
benchmarks_dirs["qsort_small"]            = home_dir + automotive_subdir + "qsort/"
benchmarks_dirs["qsort_large"]            = home_dir + automotive_subdir + "qsort/"
benchmarks_dirs["susan_small_edge"]       = home_dir + automotive_subdir + "susan/"
benchmarks_dirs["susan_small_corner"]     = home_dir + automotive_subdir + "susan/"
benchmarks_dirs["susan_small_smooth"]     = home_dir + automotive_subdir + "susan/"
benchmarks_dirs["susan_large_edge"]       = home_dir + automotive_subdir + "susan/"
benchmarks_dirs["susan_large_corner"]     = home_dir + automotive_subdir + "susan/"
benchmarks_dirs["susan_large_smooth"]     = home_dir + automotive_subdir + "susan/"
benchmarks_dirs["jpeg_small_progressive"] = home_dir + consumer_subdir + "jpeg/"
benchmarks_dirs["jpeg_large_progressive"] = home_dir + consumer_subdir + "jpeg/"
benchmarks_dirs["jpeg_small_ppm"]         = home_dir + consumer_subdir + "jpeg/"
benchmarks_dirs["jpeg_large_ppm"]         = home_dir + consumer_subdir + "jpeg/"
benchmarks_dirs["stringsearch_small"]     = home_dir + office_subdir + "stringsearch/"
benchmarks_dirs["stringsearch_large"]     = home_dir + office_subdir + "stringsearch/"
benchmarks_dirs["dijkstra_large"]         = home_dir + network_subdir + "dijkstra/"
benchmarks_dirs["dijkstra_small"]         = home_dir + network_subdir + "dijkstra/"
benchmarks_dirs["patricia_large"]         = home_dir + network_subdir + "patricia/"
benchmarks_dirs["patricia_small"]         = home_dir + network_subdir + "patricia/"
benchmarks_dirs["bf_small_enc"]           = home_dir + security_subdir + "blowfish/"
benchmarks_dirs["bf_small_dec"]           = home_dir + security_subdir + "blowfish/"
benchmarks_dirs["bf_large_enc"]           = home_dir + security_subdir + "blowfish/"
benchmarks_dirs["bf_large_dec"]           = home_dir + security_subdir + "blowfish/"
benchmarks_dirs["sha_small"]              = home_dir + security_subdir + "sha/"
benchmarks_dirs["sha_large"]              = home_dir + security_subdir + "sha/"
benchmarks_dirs["rijndael_large_enc"]     = home_dir + security_subdir + "rijndael/"
benchmarks_dirs["rijndael_large_dec"]     = home_dir + security_subdir + "rijndael/"
benchmarks_dirs["rijndael_small_dec"]     = home_dir + security_subdir + "rijndael/"
benchmarks_dirs["rijndael_small_enc"]     = home_dir + security_subdir + "rijndael/"
benchmarks_dirs["gsm_small_enc"]          = home_dir + telecomm_subdir + "gsm/"
benchmarks_dirs["gsm_small_dec"]          = home_dir + telecomm_subdir + "gsm/"
benchmarks_dirs["gsm_large_enc"]          = home_dir + telecomm_subdir + "gsm/"
benchmarks_dirs["gsm_large_dec"]          = home_dir + telecomm_subdir + "gsm/"
benchmarks_dirs["fft_large_inv"]          = home_dir + telecomm_subdir + "FFT/"
benchmarks_dirs["fft_large"]              = home_dir + telecomm_subdir + "FFT/"
benchmarks_dirs["fft_small_inv"]          = home_dir + telecomm_subdir + "FFT/"
benchmarks_dirs["fft_small"]              = home_dir + telecomm_subdir + "FFT/"
benchmarks_dirs["adpcm_small_dec"]        = home_dir + telecomm_subdir + "adpcm/"
benchmarks_dirs["adpcm_large_dec"]        = home_dir + telecomm_subdir + "adpcm/"
benchmarks_dirs["adpcm_small_enc"]        = home_dir + telecomm_subdir + "adpcm/"
benchmarks_dirs["adpcm_large_enc"]        = home_dir + telecomm_subdir + "adpcm/"
benchmarks_dirs["crc32"]                  = home_dir + telecomm_subdir + "CRC32/"


benchmarks_cmds["basicmath_large"]        = "./basicmath_large"
benchmarks_cmds["basicmath_small"]        = "./basicmath_small"
benchmarks_cmds["bitcount_small"]         = "./bitcnts 75000"
benchmarks_cmds["bitcount_large"]         = "./bitcnts 1125000"
benchmarks_cmds["qsort_small"]            = "./qsort_small input_small.dat"
benchmarks_cmds["qsort_large"]            = "./qsort_large input_large.dat"
benchmarks_cmds["susan_small_smooth"]     = "./susan input_small.pgm output_small.smoothing.pgm -s"
benchmarks_cmds["susan_small_edge"]       = "./susan input_small.pgm output_small.edges.pgm -e"
benchmarks_cmds["susan_small_corner"]     = "./susan input_small.pgm output_small.corners.pgm -c"
benchmarks_cmds["susan_large_smooth"]     = "./susan input_large.pgm output_large.smoothing.pgm -s"
benchmarks_cmds["susan_large_corner"]     = "./susan input_large.pgm output_large.corners.pgm -c"
benchmarks_cmds["susan_large_edge"]       = "./susan input_large.pgm output_large.edges.pgm -e"
benchmarks_cmds["jpeg_small_progressive"] = "./jpeg-6a/cjpeg -dct int -progressive -opt -outfile output_small_encode.jpeg input_small.ppm"
benchmarks_cmds["jpeg_small_ppm"]         = "./jpeg-6a/djpeg -dct int -ppm -outfile output_small_decode.ppm input_small.jpg"
benchmarks_cmds["jpeg_large_progressive"] = "./jpeg-6a/cjpeg -dct int -progressive -opt -outfile output_large_encode.jpeg input_large.ppm"
benchmarks_cmds["jpeg_large_ppm"]         = "./jpeg-6a/djpeg -dct int -ppm -outfile output_large_decode.ppm input_large.jpg"
benchmarks_cmds["stringsearch_small"]     = "./search_small"
benchmarks_cmds["stringsearch_large"]     = "./search_large"
benchmarks_cmds["dijkstra_large"]         = "./dijkstra_large input.dat"
benchmarks_cmds["dijkstra_small"]         = "./dijkstra_small input.dat"
benchmarks_cmds["patricia_small"]         = "./patricia small.udp "
benchmarks_cmds["patricia_large"]         = "./patricia large.udp "
benchmarks_cmds["bf_small_enc"]           = "./bf e input_small.asc output_small.enc 1234567890abcdeffedcba0987654321"
benchmarks_cmds["bf_small_dec"]           = "./bf d output_small.enc output_small.asc 1234567890abcdeffedcba0987654321"
benchmarks_cmds["bf_large_enc"]           = "./bf e input_large.asc output_large.enc 1234567890abcdeffedcba0987654321"
benchmarks_cmds["bf_large_dec"]           = "./bf d output_large.enc output_large.asc 1234567890abcdeffedcba0987654321"
benchmarks_cmds["sha_small"]              = "./sha input_small.asc"
benchmarks_cmds["sha_large"]              = "./sha input_large.asc"
benchmarks_cmds["rijndael_large_enc"]     = "./rijndael input_large.asc output_large.enc e 1234567890abcdeffedcba09876543211234567890abcdeffedcba0987654321"
benchmarks_cmds["rijndael_large_dec"]     = "./rijndael output_large.enc output_large.dec d 1234567890abcdeffedcba09876543211234567890abcdeffedcba0987654321"
benchmarks_cmds["rijndael_small_enc"]     = "./rijndael input_small.asc output_small.enc e 1234567890abcdeffedcba09876543211234567890abcdeffedcba0987654321"
benchmarks_cmds["rijndael_small_dec"]     = "./rijndael output_small.enc output_small.dec d 1234567890abcdeffedcba09876543211234567890abcdeffedcba0987654321"
benchmarks_cmds["gsm_small_enc"]          = "./bin/toast -fps -c data/small.au > output_small.encode.gsm"
benchmarks_cmds["gsm_small_dec"]          = "./bin/untoast -fps -c data/small.au.run.gsm > output_small.decode.run"
benchmarks_cmds["gsm_large_enc"]          = "./bin/toast -fps -c data/large.au > output_large.encode.gsm"
benchmarks_cmds["gsm_large_dec"]          = "./bin/untoast -fps -c data/large.au.run.gsm > output_large.decode.run"
benchmarks_cmds["fft_small"]              = "./fft 4 4096 > output_small.txt"
benchmarks_cmds["fft_small_inv"]          = "./fft 4 8192 -i > output_small.inv.txt"
benchmarks_cmds["fft_large"]              = "./fft 8 32768 > output_large.txt"
benchmarks_cmds["fft_large_inv"]          = "./fft 8 32768 -i > output_large.inv.txt"
benchmarks_cmds["adpcm_small_enc"]        = "./bin/rawcaudio < data/small.pcm > output_small.adpcm"
benchmarks_cmds["adpcm_small_dec"]        = "./bin/rawdaudio < data/small.adpcm > output_small.pcm"
benchmarks_cmds["adpcm_large_enc"]        = "./bin/rawcaudio < data/large.pcm > output_large.adpcm"
benchmarks_cmds["adpcm_large_dec"]        = "./bin/rawdaudio < data/large.adpcm > output_large.pcm"
benchmarks_cmds["crc32"]                  = "./crc ../adpcm/data/large.pcm > output_large.txt"



def main():
    if len(sys.argv) != 2:
        print "Specify action [run|stats|get]"
        return 1
    if sys.argv[1] == "run":
        run_all()
    elif sys.argv[1] == "stats":
        stats_all()
    elif sys.argv[1] == "get":
        get_all()
    else:
        print "Wrong action"
        return 1
    return 0



def valgrind_call(benchmark_name,cache_size, ways, line, policy, benchmark_cmd):

    log_name = "results_" + benchmark_name + "_" + str(cache_size) + "_" + policy + ".log"

    valgrind_cmd = "%s --tool=cachegrind --I1=%d,%d,%d --D1=%d,%d,%d --cache-policy=%s %s 2> %s 1> /dev/null &" % \
            (valgrind_dir,cache_size,ways,line,cache_size,ways,line,policy,benchmark_cmd,log_name)

    os.system(valgrind_cmd)


def run_all():
    for benchmark in benchmarks_list:
        print benchmark
        os.chdir(benchmarks_dirs[benchmark])
        for cache_policy in ["lru", "lip", "random", "fifo"]:
            for cache_size in [256,512,1024,2048,4096,8192,16384,32768,65536]:
                valgrind_call(benchmark,cache_size,ways,cache_line,cache_policy,benchmarks_cmds[benchmark])

def stats_all():
    for benchmark in benchmarks_list:
       os.system("./get_stats.sh " + benchmarks_dirs[benchmark] + " " + benchmark + " &")

def get_all():
    os.chdir(home_dir)
    find_and_copy_cmd = "find -name \"stats*\" -not -path \"" + "./" + results_dir + "/*\" " + " -exec cp \"{}\" " + results_dir  + " \; "
    os.system("mkdir -p " + results_dir )
    os.system(find_and_copy_cmd)




if __name__ == "__main__":
    sys.exit(main())

