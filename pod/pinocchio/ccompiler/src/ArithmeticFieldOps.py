from Wires import *
from FieldOps import *

class FieldZeroP(FieldOp):
        """Adds support for the zero-equals gate described in the paper"""

        def __init__(self, comment, in_wire, out_wire, m_wire):
                FieldOp.__init__(self, comment)
                self.in_wire = in_wire
                self.out_wire = out_wire
                self.m_wire = m_wire

        def field_command(self):
                return "zerop in %s out %s" % (WireList([self.in_wire]), WireList([self.m_wire, self.out_wire]))
        def input_wires(self): return WireList([self.in_wire])
        def output_wires(self): return WireList([self.out_wire])

class FieldConstMul(FieldOp):
	def __init__(self, comment, value, in_wire, out_wire):
		FieldOp.__init__(self, comment)
		self.assert_int(value)
		assert(isinstance(in_wire, Wire))
		assert(isinstance(out_wire, Wire))
		self.value = value
		self.in_wire = in_wire
		self.out_wire = out_wire

	def field_command(self):
		if (self.value >= 0):
			constant = "%x" % self.value
		else:
			constant = "neg-%x" % (-self.value)
		return "const-mul-%s in %s out %s" % (
			constant, WireList([self.in_wire]), WireList([self.out_wire]))

	def input_wires(self): return WireList([self.in_wire])
	def output_wires(self): return WireList([self.out_wire])

class FieldBinaryOp(FieldOp):
	def __init__(self, comment, verb, in_list, out_list):
		FieldOp.__init__(self, comment)
		assert(isinstance(in_list, WireList))
		assert(isinstance(out_list, WireList))
		self.verb = verb
		self.in_list = in_list
		self.out_list = out_list

	def field_command(self):
		return "%s in %s out %s" % (self.verb, self.in_list, self.out_list)

	def input_wires(self): return self.in_list
	def output_wires(self): return self.out_list

class FieldAdd(FieldBinaryOp):
	def __init__(self, comment, in_list, out_list):
		FieldBinaryOp.__init__(self, comment, "add", in_list, out_list)
		assert(len(in_list)>1)
		assert(len(out_list)==1)

class FieldMul(FieldBinaryOp):
	def __init__(self, comment, in_list, out_list):
		FieldBinaryOp.__init__(self, comment, "mul", in_list, out_list)
		assert(len(in_list)>1)
		assert(len(out_list)==1)

	def report(self, r):
		r.add("mul", 1)
		r.add("raw_mul", 1)

class FieldSplit(FieldBinaryOp):
	def __init__(self, comment, in_list, out_list):
		FieldBinaryOp.__init__(self, comment, "split", in_list, out_list)
		assert(len(in_list)==1)

	def report(self, r):
		r.add("split", 1)
		r.add("raw_mul", len(self.out_list)+1)

	def input_wires(self): return self.in_list
	def output_wires(self): return self.out_list

