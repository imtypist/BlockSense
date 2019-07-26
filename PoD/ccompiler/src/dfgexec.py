#!/usr/bin/python

import pickle
import sys
from Collapser import Collapser

#mypath = os.path.dirname(__file__)
#sys.path.append("%s/../external-code/ply-3.4" % mypath)
#sys.path.append("%s/../external-code/pycparser-2.08" % mypath)

class InputEvaluator(Collapser):
	def __init__(self, array):
		Collapser.__init__(self)
		self.array = array
		self.storage = None
		self.has_storage = False

	def get_dependencies(self, expr):
		return expr.collapse_dependencies()

	def collapse_impl(self, expr):
		return expr.evaluate(self)

	def get_input(self, sk):
		if (not self.has_storage):
			self.has_storage = True
			self.storage = sk.storage
		assert(self.storage == sk.storage)
		return self.array[sk.idx]

	def __repr__(self):
		return repr(self.array)

def main():
	(dfg_file,in_file,out_file) = sys.argv[1:]

	dfg = pickle.load(open(dfg_file))

	input_array = []
	expect_idx = 0
	ifp = open(in_file)
	for line in ifp.readlines():
		#print "line: #%s#" % line
		(file_idx, value) = line.split(" ")
		assert(int(file_idx) == expect_idx)
		expect_idx += 1
		input_array.append(int(value, 16))
	inputs = InputEvaluator(input_array)
	#print inputs

	output_values = []
	for (output_name,output_expr) in dfg:
		#print "output_name %s" % output_name
		value = inputs.collapse_tree(output_expr)
		output_values.append(value)

	ofp = open(out_file, "w")
	out_idx = 0
	for output_value in output_values:
		ofp.write("%d %x\n" % (out_idx, output_value))
		out_idx += 1
	ofp.close()

main()
sys.exit(0)
