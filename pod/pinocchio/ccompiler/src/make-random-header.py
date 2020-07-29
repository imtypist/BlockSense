#!/usr/bin/python

import sys
import random
import argparse

def big_random_number(bit_width):
	a = 0
	while (bit_width > 0):
		# we only get a float-mantissa's worth of randomness out of each call. We're
		# probably using random module incorrectly.
		b = int(random.uniform(0, 1<<bit_width))
#		print "xoring %3s bits %30x" % (bit_width, b)
		a = a^b
		bit_width -= 48
		if (bit_width < 1): break
	return a

def write_random(ofp, index, bit_width):
		ofp.write("#if RANDOM_DATA_SIZE > %d\n" % index)
		ofp.write("0x%x >> RANDOM_REDUCE,\n" % big_random_number(bit_width))
		ofp.write("#endif\n")

def write_random_range(start, end, ofp, bit_width):
	for index in range(start, end):
		write_random(ofp, index, bit_width)

def main():
	parser = argparse.ArgumentParser(description='Generate a selectable bunch of random numbers')
	parser.add_argument('hfile',
		help='file to write')
	parser.add_argument('--count', dest='count',
		help='count', default=100000)
	parser.add_argument('--bit-width', dest='bit_width',
		help='bit width', default=31)
	args = parser.parse_args()

	random.seed(19)
	ofp = open(args.hfile, "w")
	count = int(args.count)
	bit_width = int(args.bit_width)

	tokens = args.hfile.split('/')
	filename = tokens[-1]

	#Max_Entries_Per_File = 130
	Max_Entries_Per_File = 13000000
	
	if count < Max_Entries_Per_File:
		write_random_range(0, count, ofp, bit_width)
	else: # We need to break header into pieces or gcc will object to the header file size
		for entries_written in range(0, count, Max_Entries_Per_File):
			cur_index = entries_written // Max_Entries_Per_File
			subfilename = filename + ("%d" % cur_index)
			subfilepath = args.hfile + ("%d" % cur_index)
			subfile = open(subfilepath, "w")
			num_subfile_entries = min(count - entries_written, Max_Entries_Per_File)
			write_random_range(entries_written, entries_written + num_subfile_entries, subfile, bit_width) 
			subfile.close()
			
			ofp.write('#if RANDOM_DATA_SIZE > %d\n' % entries_written)
			ofp.write('#include "%s"\n' % subfilename)
			ofp.write('#endif\n')

	ofp.write("#if RANDOM_DATA_SIZE > %d\n" % count)
	ofp.write("#error not enough random material available\n")
	ofp.write("#endif\n")
	ofp.close()

main()
