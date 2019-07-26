import types
from Wires import *

class FieldOp:
	def __init__(self, comment):
		self._comment = comment

	def field_command(self): raise Exception("abstract")
	def input_wires(self): raise Exception("abstract")
	def output_wires(self): raise Exception("abstract")

	def report(self, r): pass

	def __repr__(self):
		return "%-40s # %s" % (self.field_command(), self._comment)

	def assert_int(self, value):
		assert(type(value)==types.IntType or type(value)==types.LongType)
		
class FieldInputBase(FieldOp):
	def __init__(self, command, comment, out_wire):
		FieldOp.__init__(self, comment)
		self._command = command
		assert(isinstance(out_wire, Wire))
		self.out_wire = out_wire

	def field_command(self):
		return "%s %s" % (self._command, self.out_wire)

	def input_wires(self): return WireList([])
	def output_wires(self): return WireList([self.out_wire])

class FieldInput(FieldInputBase):
	def __init__(self, comment, out_wire):
		FieldInputBase.__init__(self, "input", comment, out_wire)

class FieldNIZKInput(FieldInputBase):
	def __init__(self, comment, out_wire):
		FieldInputBase.__init__(self, "nizkinput", comment, out_wire)

class FieldOutput(FieldOp):
	def __init__(self, comment, in_wire):
		FieldOp.__init__(self, comment)
		assert(isinstance(in_wire, Wire))
		self.in_wire = in_wire

	def field_command(self):
		return "output %s" % (self.in_wire)

	def input_wires(self): return WireList([self.in_wire])
	def output_wires(self): return WireList([])
		# This line marks an output, it doesn't specify an output.
		# Otherwise, we'd detect a duplicate output.
