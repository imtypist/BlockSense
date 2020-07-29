#!/usr/bin/python

import operator
from BaseEval import *

class EAConstMul(BaseEvalGate):
	def __init__(self, const, wire_id):
		BaseEvalGate.__init__(self)
		self.const = const
		self.wire_id = wire_id

	def inputs(self):
		return [self.wire_id]

	def eval_internal(self, ae):
		input = ae.lookup(self.wire_id)
		return input*self.const

class EASplit(BaseEvalGate):
	def __init__(self, wire_id, bit, num_bits):
		BaseEvalGate.__init__(self)
		self.wire_id = wire_id
		self.bit = bit
		self.num_bits = num_bits

	def inputs(self):
		return [self.wire_id]

	def eval_internal(self, ae):
		input = ae.lookup(self.wire_id)
		assert(input >= 0)
		if self.bit == 0:
			assert(input < 2**self.num_bits)
		return (input>>self.bit) & 1

class EAZerop(BaseEvalOp):
        def __init__(self, wire_id):
                BaseEvalGate.__init__(self)
                self.wire_id = wire_id

        def inputs(self):
                return [self.wire_id]

        def eval_internal(self, ae):
                input = ae.lookup(self.wire_id)
                if(input == 0):
                        return 0
                else:
                        return 1

class EAAdd(BaseEvalOp):
	def __init__(self, wire_list):
		BaseEvalOp.__init__(self, wire_list, operator.add)

class EAMul(BaseEvalOp):
	def __init__(self, wire_list):
		BaseEvalOp.__init__(self, wire_list, operator.mul)

##############################################################################

class ArithEval(BaseEval):
	def __init__(self):
		BaseEval.__init__(self)

	def sub_parse(self, verb, in_a, out_a):
		if (verb=='split'):
			for bit in range(len(out_a)):
				self.add_gate(out_a[bit], EASplit(self.unique(in_a), bit, len(out_a)))
		elif (verb=="add"):
			self.add_gate(self.unique(out_a), EAAdd(in_a))
		elif (verb=="mul"):
			self.add_gate(self.unique(out_a), EAMul(in_a))
		elif (verb.startswith("const-mul-")):
			if (verb[10:14]=="neg-"):
				sign = -1
				value = verb[14:]
			else:
				sign = 1
				value = verb[10:]
			const = int(value, 16) * sign
			#print "line: %s" % line
			#print "const %s  in %s %s out %s %s" % (const, in_a, self.unique(in_a), out_a, self.unique(out_a))
			self.add_gate(self.unique(out_a), EAConstMul(const, self.unique(in_a)))
                elif (verb=="zerop"):
                        self.add_gate(out_a[0], EAZerop(self.unique(in_a)))
                        self.add_gate(out_a[1], EAZerop(self.unique(in_a)))
		else:
			assert(False)

ArithEval()
