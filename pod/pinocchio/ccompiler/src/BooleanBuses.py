from Buses import *
from BooleanFieldOps import *

class BooleanInputBus(BooleanBus):
	def __init__(self, board, input_idx):
		BooleanBus.__init__(self, board, MAJOR_INPUT)
		self.assert_int(input_idx)
		self.input_idx = input_idx
		self.set_order(input_idx)
		self._used = None
		self._width = self.board.bit_width.get_width()

	def __repr__(self):
		return "BInput(%s)" % self.input_idx

	def set_used(self, used):
		assert(self._used==None)
		self._used = used

	def do_trace_count(self): return self._width
	def get_wire_count(self): return self._width

	def get_field_ops(self):
		# this assertion verifies that the inputs expecting a particular
		# input storage location were, after all the fancy sorting,
		# correctly assigned the corresponding
		# Wire slots in the emitted circuit.
		assert(self.wire_list[0].idx == self.input_idx * self._width)
		cmds = []
		for j in range(self._width):
			comment = "bit %d of input %d" % (j, self.input_idx)
			if (not self._used):
				comment += " (unused)"
			cmds.append(
				FieldInput(comment, self.wire_list[j]))
		return cmds
	
	def do_trace(self, j):
		return self.wire_list[j]

class BooleanZero(BooleanBus):
	def __init__(self, board):
		Bus.__init__(self, board, MAJOR_LOGIC)

	def do_trace_count(self): return 1

	def get_wire_count(self): return 1

	def get_field_ops(self):
		return [ FieldXor("zero",
				self.board.one_wire(), self.board.one_wire(),
				self.wire_list[0]) ]

	def do_trace(self, j):
		return self.wire_list[0]

class BooleanOutputBus(BooleanBus):
	def __init__(self, board, bus_in, output_idx):
		BooleanBus.__init__(self, board, MAJOR_OUTPUT)
		# caller responsible to put outputs in order expected by C spec
		self.assert_int(output_idx)
		self.bus_in = bus_in
		self.output_idx = output_idx
		self.set_order(output_idx)
		self._width = self.board.bit_width.get_width()

	def do_trace_count(self): return self._width
	def get_wire_count(self): return self._width

	def get_field_ops(self):
		cmds = []
		for j in range(self._width):
			comment = "bit %d of output %d" % (j, self.output_idx)
			out_wire = self.wire_list[j]
			cmds.append(FieldXor(comment,
				self.board.zero_wire(), self.bus_in.get_trace(j), out_wire))
			cmds.append(FieldOutput(comment, out_wire))
		return cmds

	def do_trace(self, j):
		assert(False)	# that would be weird.

class BooleanAddBus(BooleanBus):
	def __init__(self, board, bus_left, bus_right):
		BooleanBus.__init__(self, board, MAJOR_LOGIC)
		#print "left: ", bus_left, bus_left.get_trace_type()
		assert(bus_left.get_trace_type() == BOOLEAN_TYPE)
		assert(bus_right.get_trace_type() == BOOLEAN_TYPE)
		self.bus_left = bus_left
		self.bus_right = bus_right

		max_bits = max(bus_left.get_trace_count(), bus_right.get_trace_count())
		self._trace_count = max_bits + 1

	def get_trace_count(self): return self._trace_count

	def get_wire_count(self): return 5*self._trace_count

	def get_field_ops(self):
		cmds = []
		for j in range(self._trace_count):
			xiw  = self.bus_left.get_trace(j)
			yiw  = self.bus_right.get_trace(j)
			if (j==0):
				# TODO opt use half-adder at beginning
				carryw = self.board.zero_wire()
			else:
				carryw = self.wire_list[(j-1)*5+4]

			xxw  = self.wire_list[j*5+0]
			yxw  = self.wire_list[j*5+1]
			andw = self.wire_list[j*5+2]
			sxw  = self.wire_list[j*5+3]
			cxw  = self.wire_list[j*5+4]
			nm = self.wire_list[0]
			cmds.append(FieldXor("add %s  xx%d" % (nm,j), xiw, carryw, xxw))
			cmds.append(FieldXor("add %s  yx%d" % (nm,j), yiw, carryw, yxw))
			cmds.append(FieldAnd("add %s and%d" % (nm,j), xxw, yxw, andw))
			cmds.append(FieldXor("add %s  sx%d" % (nm,j), xiw, yxw, sxw))
			cmds.append(FieldXor("add %s  cx%d" % (nm,j), carryw, andw, cxw))
		return cmds
		
	def do_trace(self, j):
		if (j==self._trace_count-1):
			assert(j*5+4 == len(self.wire_list)-1)
			return self.wire_list[-1]
		else:
			return self.wire_list[j*5+3]

class ConstantBitXorBus(ConstantBitXorBase):
	def __init__(self, board, value, bus):
		ConstantBitXorBase.__init__(self, board, value, bus)

	def wires_per_xor(self):
		return 1

	def invert_field_op(self, comment, in_wire, wires):
		return [ FieldXor(comment,
				self.board.one_wire(), in_wire, wires[0]) ]

class AllOnesBus(AllOnesBase):
	def __init__(self, board, bus):
		AllOnesBase.__init__(self, board, bus)

	def and_field_op(self, comment, inputs, outputs):
		return FieldAnd(comment, inputs[0], inputs[1], outputs[0])

class BitWiseBus(BinaryBooleanBus):
	def __init__(self, board, bus_left, bus_right, field_op_class, comment_name):
		BinaryBooleanBus.__init__(self, board, bus_left, bus_right)
		self.field_op_class = field_op_class
		self.comment_name = comment_name

	def get_wire_count(self):
		return self.get_trace_count()

	def get_field_ops(self):
		cmds = []
		for out in range(self.get_trace_count()):
			comment = "%s bit %d" % (self.comment_name, out)
			cmds.append(self.field_op_class(comment,
				self.bus_left.get_trace(out),
				self.bus_right.get_trace(out),
				self.wire_list[out]))
		return cmds

	def do_trace(self, j):
		return self.wire_list[j]

class BitAndBus(BitWiseBus):
	def __init__(self, board, bus_left, bus_right):
		BitWiseBus.__init__(self, board, bus_left, bus_right, FieldAnd, "bitand")

class BitOrBus(BitWiseBus):
	def __init__(self, board, bus_left, bus_right):
		BitWiseBus.__init__(self, board, bus_left, bus_right, FieldOr, "bitor")

class BitAndBus(BitWiseBus):
	def __init__(self, board, bus_left, bus_right):
		BitWiseBus.__init__(self, board, bus_left, bus_right, FieldAnd, "bitand")

class XorBus(BitWiseBus):
	def __init__(self, board, bus_left, bus_right):
		BitWiseBus.__init__(self, board, bus_left, bus_right, FieldXor, "xor")
