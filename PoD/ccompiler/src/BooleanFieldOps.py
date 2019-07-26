from Wires import *
from FieldOps import *

class BinaryFieldOp(FieldOp):
	def __init__(self, comment, left_wire, right_wire, out_wire, gate):
		FieldOp.__init__(self, comment)
		assert(isinstance(left_wire, Wire))
		assert(isinstance(right_wire, Wire))
		assert(isinstance(out_wire, Wire))
		self.left_wire = left_wire
		self.right_wire = right_wire
		self.out_wire = out_wire
		self.gate = gate

	def field_command(self):
		return "%s in %s out %s" % (
			self.gate,
			self.input_wires(), 
			self.output_wires())

	def input_wires(self): return WireList([self.left_wire, self.right_wire])
	def output_wires(self): return WireList([self.out_wire])

class FieldNand(BinaryFieldOp):
	def __init__(self, comment, left_wire, right_wire, out_wire):
		BinaryFieldOp.__init__(self, comment, left_wire, right_wire, out_wire, "nand")

	def report(self, r): r.add("gate", 1)

class FieldOr(BinaryFieldOp):
	def __init__(self, comment, left_wire, right_wire, out_wire):
		BinaryFieldOp.__init__(self, comment, left_wire, right_wire, out_wire, "or")

	def report(self, r): r.add("gate", 1)

class FieldAnd(BinaryFieldOp):
	def __init__(self, comment, left_wire, right_wire, out_wire):
		BinaryFieldOp.__init__(self, comment, left_wire, right_wire, out_wire, "and")

	def report(self, r): r.add("gate", 1)

class FieldXor(BinaryFieldOp):
	def __init__(self, comment, left_wire, right_wire, out_wire):
		BinaryFieldOp.__init__(self, comment, left_wire, right_wire, out_wire, "xor")

	def report(self, r): r.add("xor", 1)
