#!/usr/bin/python3

from sys import argv
import argparse

parser = argparse.ArgumentParser(description='Cache miss penalty simulation script.', add_help=True)

parser.add_argument('-i',
                    action='store',
                    dest='number_of_instructions',
                    type=int,
                    required=True,
                    help='Total number of executed instructions.')


parser.add_argument('-cpi',
                    action='store',
                    dest='cpi',
                    type=float,
                    required=True,
                    help='Cycles per instructions. Can be a float number.')

parser.add_argument('-d',
                    action='store',
                    dest='data_references',
                    type=int,
                    required=True,
                    help='Total number of data references.')

parser.add_argument('-d1_misses',
                    action='store',
                    dest='d1_misses',
                    type=int,
                    required=True,
                    help='Total number of L1 data cache misses.')

parser.add_argument('-d1_miss_penalty',
                    action='store',
                    dest='d1_miss_penalty',
                    type=int,
                    required=True,
                    help='L1 data cache miss penalty (in cycles).')
            
parser.add_argument('-d1_hit_penalty',
                    action='store',
                    dest='d1_hit_penalty',
                    type=int,
                    required=True,
                    help='L1 data cache hit penalty (in cycles).')

parser.add_argument('--version', action='version', version='1.0')

args = parser.parse_args()

def check_arguments(parser, args):
    error = False

    if(args.number_of_instructions <= 0):
        print('Incorrect parameters: Negative or zero number of instructions. Please, carefully read the help below.')
        error = True

    if(args.data_references < 0):
        print('Incorrect parameters: Negative or zero number of data references. Please, carefully read the help below.')
        error = True

    if(args.d1_misses < 0):
        print('Incorrect parameters: Negative or zero number of L1 data cache misses. Please, carefully read the help below.')
        error = True
    
    if(args.d1_miss_penalty < 0):
        print('Incorrect parameters: Negative or zero number of L1 data cache miss penalty. Please, carefully read the help below.')
        error = True

    if(args.d1_hit_penalty < 0):
        print('Incorrect parameters: Negative or zero number of L1 data cache hit penalty. Please, carefully read the help below.')
        error = True

    if(error == True):
        parser.print_help()
        parser.exit()

    return

#wcet = (I refs) x number of instructions per cycle + (D1 misses) x cache miss penalty + (D refs - D1 misses) x cache hit penalty
def l1_cache_analysis(args):
    wcet = (args.number_of_instructions * args.cpi) + (args.d1_misses * args.d1_miss_penalty) + ((args.data_references - args.d1_misses) * args.d1_hit_penalty)
    print("wcet =", wcet)

def main(args):
    print("main")
    l1_cache_analysis(args) 

if __name__ == '__main__':
    check_arguments(parser, args)
    main(args)