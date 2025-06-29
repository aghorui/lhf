#!/usr/bin/python3

import os
import sys
import random
import argparse
from enum import Enum
from typing import IO, Generator, Set

import numpy as np

class RNGType(Enum):
	UNIFORM     = "uniform"
	NORMAL      = "normal"

class Operation(Enum):
	UNION = "U"
	INTERSECTION = "I"
	DIFFERENCE = "D"

persistent_set = {}

# TODO: Use Persistent generator: build on top of previously generated sets.
# TODO: Try out Normal and Exponential Distributions
# PEAK at empty set
# Asymptote at high cardinality set
def generate_int_set_random_uniform(universe_max: int, max_set_size: int) -> Set[int]:
	size: int = random.randint(0, max_set_size)
	return set(random.sample(range(0, universe_max + 1), size))

class Data:
	normal_samples = []
	normal_samples_position = 0

def init_normal_samples(num_sets: int = 2000, max_set_size: int = 200):
	STANDARD_DEVIATION = 0.1
	MEAN = 0
	data = np.random.normal(MEAN, STANDARD_DEVIATION, num_sets * 2)
	data = data / data.max()
	data = data[data >= 0]
	data = (data * max_set_size).astype(int)

	Data.normal_samples = data
	Data.normal_samples_position = 0

def generate_normal_set_internal(size = 20, universe_max = 200):
	STANDARD_DEVIATION = universe_max // 4
	data = np.random.normal(universe_max // 2, STANDARD_DEVIATION, size)
	data = np.clip(data, 0, universe_max).astype(int)
	return data

def generate_int_set_random_normal(universe_max: int, max_set_size: int) -> Set[int]:
	size: int = Data.normal_samples[Data.normal_samples_position]
	Data.normal_samples_position += 1
	return set(generate_normal_set_internal(size, universe_max))

def container_to_str(a):
	return  f"{len(a)} " + (" ".join(map(str, a)))

def generate_operation_str(optype: Operation, arg1: Set[int], arg2: Set[int], result: Set[int]) -> str:
	return \
		optype.value           + "\n" + \
		container_to_str(arg1) + "\n" + \
		container_to_str(arg2) + "\n" + \
		container_to_str(result) + "\n"

def generate_operations(file: IO, dist: RNGType, num_operations: int, universe_max: int, max_set_size: int):
	file.write(f"{num_operations}\n");
	if dist == RNGType.NORMAL:
		init_normal_samples(num_operations * 2, max_set_size)
	for _ in range(num_operations):
		if dist == RNGType.UNIFORM:
			arg1 = generate_int_set_random_uniform(universe_max, max_set_size)
			arg2 = generate_int_set_random_uniform(universe_max, max_set_size)
		elif dist == RNGType.NORMAL:
			arg1 = generate_int_set_random_normal(universe_max, max_set_size)
			arg2 = generate_int_set_random_normal(universe_max, max_set_size)
		else:
			raise RuntimeError("Unknown case")
		optype: Operation = random.choice([ x for x in Operation ])
		result = {}

		if optype == Operation.UNION:
			result = arg1.union(arg2)
		elif optype == Operation.INTERSECTION:
			result = arg1.intersection(arg2)
		elif optype == Operation.DIFFERENCE:
			result = arg1.difference(arg2)

		file.write(generate_operation_str(optype, arg1, arg2, result))

def main():
	parser = argparse.ArgumentParser(
		description="Generate testing input for LatticeHashForest")

	parser.add_argument(
		"--count",
		type=int,
		required=True,
		help="Number of operations to perform")

	parser.add_argument(
		"--maxsize",
		type=int,
		required=True,
		help="Maximum size of problem sets")

	parser.add_argument(
		"--maxval",
		type=int,
		required=True,
		help="Maximum unique values for elements")

	parser.add_argument(
		"--dist",
		choices=[ "uniform", "normal" ],
		type=str,
		required=False,
		help="Type of random distribution to use",
		default="uniform")

	parser.add_argument(
		"filepath",
		type=str,
		nargs='?',
		help="File path (Optional)",
		default=None)

	args = parser.parse_args()

	file = sys.stdout

	if args.filepath != None:
		file = open(args.filepath, "w")

	num_operations  = args.count
	universe_max    = args.maxval
	set_length_max  = args.maxsize
	dist_type       = RNGType.NORMAL if args.dist == "normal" else RNGType.UNIFORM

	generate_operations(file, dist_type, num_operations, universe_max, set_length_max)

if __name__ == '__main__':
	main()