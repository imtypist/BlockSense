from Buses import *
from ArithmeticFieldOps import *


##############################################################################
# Arithmetic operators
##############################################################################

class ArithmeticBus(Bus):
	def __init__(self, board, major):
		Bus.__init__(self, board, major)

	def get_trace_type(self):
		return ARITHMETIC_TYPE

	def do_trace_count(self):
		return 1

	def do_trace(self, j):
		assert(0==j)
		return self.wire_list[-1]

class ArithZero(ArithmeticBus):
	def __init__(self, board):
		Bus.__init__(self, board, MAJOR_LOGIC)

	def get_wire_count(self): return 1

	def get_field_ops(self):
		return [ FieldConstMul(
			"zero", 0, self.board.one_wire(), self.wire_list[0]) ]

	def do_trace(self, j):
		return self.wire_list[0]

class ArithmeticInputBaseBus(ArithmeticBus):
	def __init__(self, field_class, major, board, input_idx):
		ArithmeticBus.__init__(self, board, major)
		self.field_class = field_class
		self.assert_int(input_idx)
		self.input_idx = input_idx
		self.set_order(input_idx)
		self._used = None

	def set_used(self, used):
		assert(self._used==None)
		self._used = used

	def get_active_bits(self):
		return self.board.bit_width.get_width()

	def get_wire_count(self):
		return 1

	def get_field_ops(self):
		# these assertions verify that the inputs expecting a particular
		# input storage location were, after all the fancy sorting,
		# correctly assigned the corresponding
		# Wire slots in the emitted arithmetic circuit.
		assert(len(self.wire_list)==1)
		#assert(self.wire_list[0].idx == self.input_idx)
			# not true for nizk_inputs, which are offset by the size
			# of the regular inputs.
		#print "my wire is %s, my input_idx is %s" % (self.wire_list[0].idx, self.input_idx)
		#print "type(wire) = %s" % self.wire_list[0].__class__
		comment = "input"
		if (not self._used):
			comment += " (unused)"
		return [ self.field_class(comment, Wire(self.wire_list[0].idx)) ]

class ArithmeticInputBus(ArithmeticInputBaseBus):
	def __init__(self, board, input_idx):
		ArithmeticInputBaseBus.__init__(self, FieldInput, MAJOR_INPUT, board, input_idx)

class ArithmeticNIZKInputBus(ArithmeticInputBaseBus):
	def __init__(self, board, input_idx):
		ArithmeticInputBaseBus.__init__(self, FieldNIZKInput, MAJOR_INPUT_NIZK, board, input_idx)

class ArithmeticOutputBus(ArithmeticBus):
	def __init__(self, board, bus_in, output_idx):
		ArithmeticBus.__init__(self, board, MAJOR_OUTPUT)
		# caller responsible to put outputs in order expected by C spec
		self.assert_int(output_idx)
		self.bus_in = bus_in
		self.set_order(output_idx)

	# TODO minor optimization opportunity: skip the
	# 1-mul if this bus isn't shared, which is likely the common case.

	def get_wire_count(self):
		return 1

	def get_field_ops(self):
		fm = FieldMul("output-cast",
				WireList([self.board.one_wire(), self.bus_in.get_trace(0)]),
				WireList([self.wire_list[0]]))
		fo = FieldOutput("", self.wire_list[0])
		return [ fm, fo ]

class OneInputBus(ArithmeticBus):
	def __init__(self, board):
		ArithmeticBus.__init__(self, board, MAJOR_INPUT_ONE)

	def get_wire_count(self):
		return 1

class ArithmeticZeroPBus(ArithmeticBus):
        def __init__(self, board, in_bus, in_bus_b):
                ArithmeticBus.__init__(self, board, MAJOR_LOGIC)
                self.board = board
                self.in_bus = in_bus
                self.in_bus_b = in_bus_b

        def get_active_bits(self):
                return 1

        def get_wire_count(self):
                return 6

        def get_field_ops(self):
                negb = FieldConstMul("zerop subtract negative", -1, self.in_bus_b.get_trace(0), self.wire_list[0])
                diff = FieldAdd("zerop diff",
                                WireList([self.in_bus.get_trace(0), self.wire_list[0]]),
                                WireList([self.wire_list[1]]))

                fzp = FieldZeroP("zerop %s" % self.in_bus,
                                   self.wire_list[1],
                                   self.wire_list[2], self.wire_list[3])
                inv = FieldConstMul("zerop inverse",
                                    -1,
                                    self.wire_list[2],
                                    self.wire_list[4])
                res = FieldAdd("zerop result",
                               WireList([self.board.one_wire(),
                                         self.wire_list[4]]),
                               WireList([self.wire_list[5]]))
                return [negb, diff, fzp, inv, res]

class ConstantArithmeticBus(ArithmeticBus):
	# A bus that returns a constant ARITHMETIC value
	# NB this emits a line even for the Constant 1, but it's a const-mul,
	# which is free.
	def __init__(self, board, value):
		Bus.__init__(self, board, MAJOR_LOGIC)
		self.assert_int(value)
		self.value = value

	def get_active_bits(self):
		return ceillg2(self.value)

	def get_wire_count(self):
		return 1

	def get_field_ops(self):
		return [ FieldConstMul(
				"constant %d" % self.value,
				self.value, self.board.one_wire(), self.wire_list[0]) ]

class ConstantMultiplyBus(ArithmeticBus):
	def __init__(self, board, value, bus):
		Bus.__init__(self, board, MAJOR_LOGIC)
		self.assert_int(value)
		assert(bus.get_trace_type()==ARITHMETIC_TYPE)
		self.value = value & self.board.bit_width.get_neg1()
		self.bus = bus
		self._active_bits = ceillg2(self.value)+self.bus.get_active_bits()

	def get_active_bits(self):
		return self._active_bits

	def get_wire_count(self):
		return 1

	def get_field_ops(self):
		return [ FieldConstMul(
				"multiply-by-constant %d" % self.value,
				self.value, self.bus.get_trace(0), self.wire_list[0]) ]

class ArithmeticConditionalBus(ArithmeticBus):
	def __init__(self, board, buscond, bustrue, busfalse):
		Bus.__init__(self, board, MAJOR_LOGIC)
                print buscond
                print buscond.get_trace_count()
		assert(buscond.get_trace_type()==BOOLEAN_TYPE)
		assert(buscond.get_trace_count()==1)
		assert(bustrue.get_trace_type()==ARITHMETIC_TYPE)
		assert(busfalse.get_trace_type()==ARITHMETIC_TYPE)
		self.buscond = buscond
		self.bustrue = bustrue
		self.busfalse = busfalse
		self._active_bits = max(
			bustrue.get_active_bits(), busfalse.get_active_bits())

	def get_active_bits(self):
		return self._active_bits

	def get_wire_count(self):
		return 5

	def get_field_ops(self):
		trueterm = self.wire_list[0]
		minuscond = self.wire_list[1]
		negcond = self.wire_list[2]
		falseterm = self.wire_list[3]
		result = self.wire_list[4]
		cmds = [
			FieldMul("cond trueterm",
				WireList([self.buscond.get_trace(0), self.bustrue.get_trace(0)]),
				WireList([trueterm])),
			FieldConstMul("cond minuscond",
				-1,
				self.buscond.get_trace(0),
				minuscond),
			FieldAdd("cond negcond",
				WireList([self.board.one_wire(), minuscond]),
				WireList([negcond])),
			FieldMul("cond falseterm",
				WireList([negcond, self.busfalse.get_trace(0)]),
				WireList([falseterm])),
			FieldAdd("cond result",
				WireList([trueterm, falseterm]),
				WireList([result]))
			]
		return cmds

class BinaryArithmeticBus(ArithmeticBus):
	def __init__(self, board, bus_left, bus_right, _active_bits):
		ArithmeticBus.__init__(self, board, MAJOR_LOGIC)
		assert(bus_left.get_trace_type()==ARITHMETIC_TYPE)
		assert(bus_right.get_trace_type()==ARITHMETIC_TYPE)
		self.assert_int(_active_bits)
		self.bus_left = bus_left
		self.bus_right = bus_right
		self._active_bits = _active_bits

	def get_wire_count(self): return 1

	def get_active_bits(self):
		return self._active_bits

class ArithAddBus(BinaryArithmeticBus):
	def __init__(self, board, comment, bus_left, bus_right):
		self.comment = comment
		max_bits = max(bus_left.get_active_bits(), bus_right.get_active_bits())
		_active_bits = max_bits + 1
		BinaryArithmeticBus.__init__(self, board, bus_left, bus_right, _active_bits)

	def get_field_ops(self):
		return [ FieldAdd(
				repr(self.comment),
				WireList([self.bus_left.get_trace(0),
					self.bus_right.get_trace(0)]),
				self.wire_list) ]

class ArithMultiplyBus(BinaryArithmeticBus):
	def __init__(self, board, bus_left, bus_right):
		_active_bits = bus_left.get_active_bits()+bus_right.get_active_bits()
		BinaryArithmeticBus.__init__(self, board, bus_left, bus_right, _active_bits)

	def get_field_ops(self):
		return [ FieldMul(
				"multiply",
				WireList([self.bus_left.get_trace(0),
					self.bus_right.get_trace(0)]),
				self.wire_list) ]

##############################################################################
# Conversion operators
##############################################################################

class JoinBus(Bus):
	# Convert a BOOLEAN_TYPE bus into an ARITHMETIC_TYPE bus
	def __init__(self, board, input_bus):
		Bus.__init__(self, board, MAJOR_LOGIC)
		assert(input_bus.get_trace_type()==BOOLEAN_TYPE)
		self.input_bus = input_bus
		self._active_bits = input_bus.get_trace_count()

		# NB if this is a 1-bit join, it's just a change of trace type
		# from BOOLEAN to ARITHMETIC. So we'll generate no wires or
		# field ops at all, and just connect directly to the input trace,
		# now treating it as a 1-active-bit arithmetic bus.

	def get_trace_type(self):
		return ARITHMETIC_TYPE

	def get_active_bits(self):
		return self._active_bits

	def do_trace_count(self):
		return 1

	def get_wire_count(self):
		return 2*(self._active_bits-1)

	def get_field_ops(self):
		in_width = self._active_bits
		cmds = []
		comment = "join"
		for biti in range(in_width-1):
			bit = in_width - 1 - biti
			if (biti == 0):
				prevwire = self.input_bus.get_trace(bit)
				mulcomment = " source bit %s" % bit
			else:
				prevwire = self.wire_list[biti*2 - 1]
				mulcomment = ""

			factor_output = self.wire_list[biti*2]
			cmds.append(
				FieldConstMul(comment+mulcomment, 2, prevwire, factor_output))

			addcomment = " source bit %s" % (bit-1)
			term_in_list = WireList([
				self.input_bus.get_trace(bit-1),
				factor_output])
			term_output = WireList([self.wire_list[biti*2 + 1]])
			cmds.append(
				FieldAdd(comment+addcomment, term_in_list, term_output))
		return cmds

	def do_trace(self, j):
		if (self._active_bits==1):
			return self.input_bus.get_trace(0)
		else:
			return self.wire_list[-1]

class SplitBus(Bus):
	# Convert an ARITHMETIC_TYPE bus into a BOOLEAN_TYPE bus
	def __init__(self, board, input_bus):
		Bus.__init__(self, board, MAJOR_LOGIC)
                print input_bus
		assert(input_bus.get_trace_type()==ARITHMETIC_TYPE)
		self.input_bus = input_bus
		self._trace_count = self.board.bit_width.truncate(
			self.input_bus.get_active_bits())

		# NB if this is a 1-bit split, it's just a change of trace type
		# from ARITHMETIC to BOOLEAN. So we'll generate no wires or
		# field ops at all, and just connect directly to the input trace,
		# known to be 1-active-bit (and hence {0,1}-valued), so we
		# can just treat it as a 1-trace boolean bus.

	def __repr__(self):
		return "SplitBus"

	def get_trace_type(self):
		return BOOLEAN_TYPE

	def do_trace_count(self):
		return self._trace_count
	
	def get_wire_count(self):
		# Nota Extremely Bene: we need to split on the number of bits
		# in the incoming arithmetic value, NOT on our outgoing (possibly-
		# truncated) trace_count. The underlying split math only works on
		# the assumption that the *input* is smaller than the split size.
		if (self.input_bus.get_active_bits()==1):
			return 0
		else:
			return self.input_bus.get_active_bits()

	def get_field_ops(self):
		if (self.input_bus.get_active_bits()==1):
			return []
		else:
			return [ FieldSplit(self,
					WireList([self.input_bus.get_trace(0)]),
					self.wire_list) ]

	def do_trace(self, j):
		if (self.input_bus.get_active_bits()==1):
			return self.input_bus.get_trace(0)
		else:
			return self.wire_list[j]

##############################################################################
# Boolean operations with arithmetic field op implementations
##############################################################################

class ConstantBitXorBus(ConstantBitXorBase):
	def __init__(self, board, value, bus):
		ConstantBitXorBase.__init__(self, board, value, bus)

	def wires_per_xor(self):
		return 2

	def invert_field_op(self, comment, in_wire, wires):
		(minus_wire, xor_wire) = wires
		return [
			FieldConstMul(comment, -1, in_wire, minus_wire),
			FieldAdd(comment,
				WireList([self.board.one_wire(), minus_wire]),
				WireList([xor_wire]))
			]

class AllOnesBus(AllOnesBase):
	def __init__(self, board, bus):
		AllOnesBase.__init__(self, board, bus)

	def and_field_op(self, comment, inputs, outputs):
		return FieldMul(comment, inputs, outputs)

class BitAndBus(BinaryBooleanBus):
	def __init__(self, board, bus_left, bus_right):
		BinaryBooleanBus.__init__(self, board, bus_left, bus_right)

	def get_wire_count(self):
		return self.get_trace_count()

	def do_trace(self, j):
		return self.wire_list[j]

	def get_field_ops(self):
		cmds = []
		for out in range(self.get_trace_count()):
			comment = "bitand bit %d" % out
			cmds.append(FieldMul(comment,
				WireList([self.bus_left.get_trace(out),
					self.bus_right.get_trace(out)]),
				WireList([self.wire_list[out]])))
		return cmds


class EqlBusArith(ArithmeticBus):
        """ Support for == using the zero-equal field operation"""
        def __init__(self, board, bus_left, bus_right):
                ArithmeticBus.__init__(self, board, MAJOR_LOGIC)
                assert(bus_left.get_trace_count() == bus_right.get_trace_count())
                self.board = board
                self.bus_left = bus_left
                self.bus_right = bus_right
                self._trace_count = 1

        def do_trace_count(self):
                return self._trace_count

        def get_wire_count(self):
                """Only 3 wires are needed, because the operation is (zerop (+ left (* -1 right)))"""
                return 3

        def get_trace_type(self):
                # This seems to trigger a conversion to boolean *inputs*, when we really only need a boolean output.

                return BOOLEAN_TYPE

        def get_field_ops(self):
                cmds = []
                comment = "Eql "
                rightneg = self.wire_list[0]
                cmds.append(FieldConstMul(comment+"-1 * right", -1, self.bus_right.do_trace(0), rightneg))
                leftplusright = self.wire_list[1]
                cmds.append(FieldAdd(comment + "left + (-1 * right)",
                                     WireList([self.bus_left.get_trace(0), rightneg]),
                                     WireList([leftplusright])))

                result = self.wire_list[2]
                cmds.append(FieldZeroP(comment + "zerop(left + (-1 * right))",
                                       leftplusright,
                                       result))
                return cmds

class EqlBusBoolean(BooleanBus):
        """Support for == using boolean operations implemented in the field"""

        def __init__(self, board, bus_left, bus_right):
                BooleanBus.__init__(self, board, MAJOR_LOGIC)
                assert(bus_left.get_trace_count() == bus_right.get_trace_count())
                self.bus_left = bus_left
                self.bus_right = bus_right
                self._trace_count = 1
                self.wire_cnd = None
                self.board = board

        def do_trace_count(self):
                return self._trace_count

        def get_wire_count(self): return self.bus_left.get_trace_count()*7-1

        def get_field_ops(self): 
                cmds = []
                for tmp in range(self.bus_left.get_trace_count()):
                        # Xor the left with the right, then compute AND tree
                        cmds.extend(make_xnor(self.board, self.bus_left, self.bus_right, tmp, self.wire_list[6*tmp:6*(tmp+1)]))
                cmds.append(FieldMul("AND tree for EqlBus bit 0-1", 
                                     WireList([self.wire_list[5], self.wire_list[11]]),
                                     WireList([self.wire_list[6*self.bus_left.get_trace_count()]])))
                for tmp in range(self.bus_left.get_trace_count()-2):
                        # AND tree
                        cmds.append(FieldMul("AND tree for EqlBus bit %d " % (tmp+2),
                                             WireList([self.wire_list[6*(tmp+3)-1],
                                                       self.wire_list[6*self.bus_left.get_trace_count() + tmp]]),
                                             WireList([self.wire_list[6*self.bus_left.get_trace_count() + tmp + 1]])))
                return cmds

        def do_trace(self, j):
                assert(j == 0)
                return self.wire_list[7*self.bus_left.get_trace_count()-2]

def make_xnor(board, left_bus, right_bus, j, wire_list):
        cmds = []
        comment = "XNOR bit %d " % j
        aplusb = wire_list[0]
        cmds.append(FieldAdd(comment+"a+b", WireList([left_bus.get_trace(j),
                                                      right_bus.get_trace(j)]),
                             WireList([aplusb])))
        ab = wire_list[1]
        cmds.append(FieldMul(comment+"ab",
                             WireList([left_bus.get_trace(j),
                                       right_bus.get_trace(j)]),
                             WireList([ab])))
        minus2ab = wire_list[2]
        cmds.append(FieldConstMul(comment+"-2ab",
                                  -2, ab, minus2ab))
        xor = wire_list[3]
        cmds.append(FieldAdd(comment+"(a+b)-2ab",
                             WireList([aplusb, minus2ab]),
                             WireList([xor])))
        neg = wire_list[4]
        cmds.append(FieldConstMul(comment+"-1 * ((a + b) - 2ab)",
                                  -1, xor, neg))
        result = wire_list[5]
        cmds.append(FieldAdd(comment+"1 - ((a+b) - 2ab)",
                             WireList([board.one_wire(), neg]),
                    WireList([result])))
        return cmds

class OrFamilyBus(BinaryBooleanBus):
	def __init__(self, board, bus_left, bus_right, product_coeff, c_name):
		BinaryBooleanBus.__init__(self, board, bus_left, bus_right)
		self.product_coeff = product_coeff
		self.c_name = c_name

	def get_wire_count(self):
		return 4*self.get_trace_count()

	def get_field_ops(self):
		cmds = []
		for out in range(self.get_trace_count()):
			comment = "%s bit %d " % (self.c_name, out)
			# (a+b)-k(ab)
			aplusb = self.wire_list[out*4]
			cmds.append(FieldAdd(comment+"a+b",
				WireList([self.bus_left.get_trace(out),
					self.bus_right.get_trace(out)]),
				WireList([aplusb])))
			ab = self.wire_list[out*4+1]
			cmds.append(FieldMul(comment+"ab",
				WireList([self.bus_left.get_trace(out),
					self.bus_right.get_trace(out)]),
				WireList([ab])))
			minus2ab = self.wire_list[out*4+2]
			cmds.append(FieldConstMul(comment+"%sab" % self.product_coeff,
				self.product_coeff, ab, minus2ab))
			result = self.wire_list[out*4+3]
			cmds.append(FieldAdd(comment+"(a+b)%sab" % self.product_coeff,
				WireList([aplusb, minus2ab]),
				WireList([result])))
		return cmds

	def do_trace(self, j):
		return self.wire_list[j*4+3]

class BitOrBus(OrFamilyBus):
	def __init__(self, board, bus_left, bus_right):
		OrFamilyBus.__init__(self, board, bus_left, bus_right, -1, "or")

class XorBus(OrFamilyBus):
	def __init__(self, board, bus_left, bus_right):
		OrFamilyBus.__init__(self, board, bus_left, bus_right, -2, "xor")

			
