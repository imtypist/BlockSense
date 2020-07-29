#!/usr/bin/python

import sys
import re
import operator
import argparse
from BaseEval import *

sys.setrecursionlimit(60)

class EBXor(BaseEvalOp):
	def __init__(self, wire_list):
		BaseEvalOp.__init__(self, wire_list, operator.xor)

class EBAnd(BaseEvalOp):
	def __init__(self, wire_list):
		BaseEvalOp.__init__(self, wire_list, operator.and_)

class EBNand(BaseEvalOp):
	def __init__(self, wire_list):
		BaseEvalOp.__init__(self, wire_list, self.nand)
	def nand(self, a, b): return not (a and b)

class EBOr(BaseEvalOp):
	def __init__(self, wire_list):
		BaseEvalOp.__init__(self, wire_list, operator.or_)

##############################################################################

class BoolEval(BaseEval):
	def __init__(self):
		BaseEval.__init__(self)

	def sub_parse(self, verb, in_a, out_a):
		if (verb=='xor'):
			self.add_gate(self.unique(out_a), EBXor(in_a))
		elif (verb=='and'):
			self.add_gate(self.unique(out_a), EBAnd(in_a))
		elif (verb=='nand'):
			self.add_gate(self.unique(out_a), EBNand(in_a))
		elif (verb=='or'):
			self.add_gate(self.unique(out_a), EBOr(in_a))
		else:
			raise Exception("Unknown verb in bool file: %s" % verb)

BoolEval()
