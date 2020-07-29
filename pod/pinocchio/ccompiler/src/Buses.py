import types
from TraceType import *
from ceillg2 import *
from FieldOps import *
#from ArithmeticFieldOps import *	# TODO plane bus zero depends on this; probably broken

MAJOR_INPUT=0
MAJOR_INPUT_ONE=1
MAJOR_INPUT_NIZK=2
MAJOR_LOGIC=3
MAJOR_OUTPUT=4

class Bus:
	def __init__(self, board, major):
		assert(board.is_board())
		self.board = board
		self._major = major
		self._order = self.board.assign_order()

	def set_order(self, minor_order):
		self._order = minor_order

	def _tuple(self):
		return (self._major, self._order)

	def __hash__(self):
		return hash(self._tuple())

	def __cmp__(self, other):
		return cmp(self._tuple(), other._tuple())

	def assign_wires(self, wire_list):
		assert(isinstance(wire_list, WireList))
		self.wire_list = wire_list

	def get_trace_type(self):	raise Exception("abstract in %s" % self)
	def get_active_bits(self):	raise Exception("abstract in %s" % self)
		# needs only be defined on trace_type ARITHMETIC_TYPE.
	def do_trace_count(self):	raise Exception("abstract in %s" % self)
	def get_wire_count(self):	raise Exception("abstract in %s" % self)
	def do_trace(self, j):		raise Exception("abstract in %s" % self)
	def get_field_ops(self):	raise Exception("abstract in %s" % self)

	def get_trace_count(self):
		# if trace_type==ARITHMETIC_TYPE, trace_count should be 1.
		result = self.do_trace_count()
		assert(self.get_trace_type()==BOOLEAN_TYPE or result==1)
		return result

	def get_trace(self, j):
		assert(0<=j)
		if (j<self.get_trace_count()):
			result = self.do_trace(j)
			assert(isinstance(result, Wire))
			return result
		else:
			return self.board.zero_wire()

	def assert_int(self, value):
		assert(type(value)==types.IntType or type(value)==types.LongType)

class OneBus(Bus):
	# This bus is only here as a placeholder to allocate the one wire,
	# which is snagged and fed directly into FieldOps. It's not used
	# like an ordinary bus; it's only a bus so we enumerate it to get
	# its field_ops at emit time.
	def __init__(self, board):
		Bus.__init__(self, board, MAJOR_INPUT_ONE)

	def get_wire_count(self):
		return 1

	def get_one_wire(self):
		return self.wire_list[0]

	def get_field_ops(self):
		return [ FieldInput("one-input", self.wire_list[0]) ]

	def do_trace_count(self):
		raise Exception("Not really a bus!")

	def do_trace(self, j):
		raise Exception("Not really a bus!")

##############################################################################
# Boolean operators
##############################################################################

class BooleanBus(Bus):
	def __init__(self, board, major):
		Bus.__init__(self, board, major)

	def get_trace_type(self):
		return BOOLEAN_TYPE

	def get_active_bits(self):
		return self.get_trace_count()

class ZeroBus(BooleanBus):
	def __init__(self, board):
		BooleanBus.__init__(self, board, MAJOR_LOGIC)

	def do_trace_count(self):
		# emitting zero here ensures that we don't needlessly widen
		# any value we might combine with. Not that we combine with any values.
		return 0

	def do_trace(self, j):
		assert(False)	# we always get zero-extended by Bus.get_trace

class ConstantBooleanBus(BooleanBus):
	# A bus that returns a constant BOOLEAN value
	def __init__(self, board, value):
		BooleanBus.__init__(self, board, MAJOR_LOGIC)
		self.assert_int(value)
		self.value = value

	def do_trace_count(self):
		return ceillg2(self.value)

	def get_wire_count(self): return 0
	def get_field_ops(self): return []
	
	def do_trace(self, j):
		if (j < self.do_trace_count()):
			bitval = (self.value >> j) & 1
		else:
			bitval = 0
		if (bitval==1):
			return self.board.one_wire()
		else:
			return self.board.zero_wire()

class ConstBitAndBus(BooleanBus):
	def __init__(self, board, value, bus):
		BooleanBus.__init__(self, board, MAJOR_LOGIC)
		assert(bus.get_trace_type()==BOOLEAN_TYPE)
		self.value = value
		self.bus = bus
		self._trace_count = min(ceillg2(self.value), bus.get_trace_count())

	def do_trace_count(self): return self._trace_count
	def get_wire_count(self): return 0
	def get_field_ops(self): return []

	def do_trace(self, j):
		if ((self.value >> j) & 1):
			return self.bus.do_trace(j)
		else:
			return self.board.zero_wire()

class ConstBitOrBus(BooleanBus):
	def __init__(self, board, value, bus):
		BooleanBus.__init__(self, board, MAJOR_LOGIC)
		assert(bus.get_trace_type()==BOOLEAN_TYPE)
		self.value = value
		self.bus = bus
		self._trace_count = max(ceillg2(self.value), bus.get_trace_count())

	def do_trace_count(self): return self._trace_count
	def get_wire_count(self): return 0
	def get_field_ops(self): return []

	def do_trace(self, j):
		if ((self.value >> j) & 1):
			return self.board.one_wire()
		else:
			return self.bus.do_trace(j)

class ConstantBitXorBase(BooleanBus):
	def __init__(self, board, value, bus):
		BooleanBus.__init__(self, board, MAJOR_LOGIC)
		assert(bus.get_trace_type()==BOOLEAN_TYPE)
		self.value = value
		self.bus = bus
		self._make_bit_map()
		self._trace_count = max(ceillg2(self.value), bus.get_trace_count())

	def _make_bit_map(self):
		self._bit_map = {}
		_val = self.value
		biti = 0
		count = 0
		while (_val != 0):
			if (_val & 1):
				self._bit_map[biti] = count
				count += 1
			biti += 1
			_val = _val >> 1

	def do_trace_count(self):
		return self._trace_count

	def get_wire_count(self):
		return self.wires_per_xor()*len(self._bit_map)

	def _bit_value(self, j):
		return ((self.value >> j) & 1)

	def wires_per_xor(self):
		raise Exception("abstract")

	def invert_field_op(self, comment, inputs, wires):
		# len(wires) == wires_per_xor()
		# emit output on wires[-1]
		raise Exception("abstract")

	def get_field_ops(self):
		cmds = []
		for biti in range(self._trace_count):
			if (biti in self._bit_map):
				# need to xor this field
				count = self._bit_map[biti]
				k = self.wires_per_xor()

				cmds.extend(self.invert_field_op(
					"bitxor bit %d" % biti,
					self.bus.get_trace(biti),
					self.wire_list[count*k:(count+1)*k]))
		return cmds

	def do_trace(self, j):
		if (j in self._bit_map):
			count = self._bit_map[j]
			k = self.wires_per_xor()
			return self.wire_list[(count+1)*k-1]
		else:
			return self.bus.do_trace(j)

class AllOnesBase(BooleanBus):
	# Test if all the inputs bits are ones. (A big wide AND gate.)
	# We build LogicalNot as AllOnes(BitNot(x)):
	# AllOnesBus(BitNot(000)==111) = 1 :: Not(0)=1
	# AllOnesBus(BitNot(010)==101) = 0 :: Not(2)=0
	# (And BitNot is just ConstantBitXorBus with neg1)
	def __init__(self, board, bus):
		BooleanBus.__init__(self, board, MAJOR_LOGIC)
		assert(bus.get_trace_type()==BOOLEAN_TYPE)
		self.bus = bus
		self._wire_count = bus.get_trace_count()-1

	def do_trace_count(self): return 1
	def get_wire_count(self): return self._wire_count

	def and_field_op(self, comment, inputs, wires):
		raise Exception("abstract")

	def get_field_ops(self):
		cmds = []
		prev_wire = self.bus.get_trace(0)
		for j in range(1, self.bus.get_trace_count()):
			out_wire = self.wire_list[j-1]
			cmds.append(self.and_field_op("all_ones",
				WireList([prev_wire,
					self.bus.get_trace(j)]),
				WireList([out_wire])))
			prev_wire = out_wire
		return cmds
	
	def do_trace(self, j):
		return self.wire_list[-1]

class LeftShiftBus(BooleanBus):
	def __init__(self, board, bus, left_shift):
		BooleanBus.__init__(self, board, MAJOR_LOGIC)
		self.assert_int(left_shift)
		assert(bus.get_trace_type()==BOOLEAN_TYPE)
		self.bus = bus
		self.left_shift = left_shift
		self._trace_count = max(0, bus.get_trace_count() + left_shift)
		self._trace_count = self.board.bit_width.truncate(self._trace_count)

	def do_trace_count(self):
		return self._trace_count

	def get_wire_count(self): return 0
	def get_field_ops(self): return []
	
	def do_trace(self, j):
		parent_bit = j - self.left_shift
		if (parent_bit < 0):
			return self.board.zero_wire()
		elif (j >= self._trace_count):
			return self.board.zero_wire()
		else:
			return self.bus.get_trace(parent_bit)

class BinaryBooleanBus(BooleanBus):
	def __init__(self, board, bus_left, bus_right):
		BooleanBus.__init__(self, board, MAJOR_LOGIC)
		assert(bus_left.get_trace_type()==BOOLEAN_TYPE)
		assert(bus_right.get_trace_type()==BOOLEAN_TYPE)
		self.bus_left = bus_left
		self.bus_right = bus_right
		self._trace_count = max(
			self.bus_left.get_trace_count(), self.bus_right.get_trace_count())

	def get_trace_type(self):
		return BOOLEAN_TYPE

	def do_trace_count(self):
		return self._trace_count
