from ArithBuses import *
from TraceType import *
import DFG
from BusReq import *

##############################################################################
# Arithmetic operators
##############################################################################

class ArithmeticInputBaseReq(BusReq):
	def __init__(self, bus_class, reqfactory, expr, type):
		BusReq.__init__(self, reqfactory, expr, type)
		self.bus_class = bus_class

	def natural_type(self):
		return ARITHMETIC_TYPE

	def natural_dependencies(self):
		return []

	def natural_impl(self):
		return self.bus_class(self.board(), self.expr.storage_key.idx)

class ArithmeticInputReq(ArithmeticInputBaseReq):
	def __init__(self, reqfactory, expr, type):
		ArithmeticInputBaseReq.__init__(self, ArithmeticInputBus, reqfactory, expr, type)

class ArithmeticNIZKInputReq(ArithmeticInputBaseReq):
	def __init__(self, reqfactory, expr, type):
		ArithmeticInputBaseReq.__init__(self, ArithmeticNIZKInputBus, reqfactory, expr, type)

class AddReq(BinaryOpReq):
	def __init__(self, reqfactory, expr, type):
		BinaryOpReq.__init__(self, reqfactory, expr, type)

	def natural_type(self): return ARITHMETIC_TYPE

	def has_const_opt(self): return False
		# we could implement a constant optimization here, but it doesn't
		# actually reduce any muls, so why bother?

	def var_impl(self, *busses):
		return ArithAddBus(self.board(), self, *busses)

class MultiplyReq(BinaryOpReq):
	def __init__(self, reqfactory, expr, type):
		BinaryOpReq.__init__(self, reqfactory, expr, type)

	def natural_type(self): return ARITHMETIC_TYPE

	def has_const_opt(self): return True

	def const_impl(self, const_expr, variable_bus):
		return ConstantMultiplyBus(self.board(), const_expr.value, variable_bus)

	def var_impl(self, *busses):
		return ArithMultiplyBus(self.board(), *busses)

class NegateReq(BusReq):
	def __init__(self, reqfactory, expr, type):
		BusReq.__init__(self, reqfactory, expr, type)

	def natural_type(self): return ARITHMETIC_TYPE

	def _req(self):
		return self.make_req(self.expr.expr, self.natural_type())

	def natural_dependencies(self):
		return [ self._req() ]

	def natural_impl(self):
		sub_bus = self.get_bus_from_req(self._req())
		return ConstantMultiplyBus(self.board(), -1, sub_bus)

##############################################################################
# ConditionalReq operator.
# accepts a boolean condition and two arithmetic inputs;
# emits an arithmetic output.
##############################################################################

class ConditionalReq(BusReq):
	def __init__(self, reqfactory, expr, type):
		BusReq.__init__(self, reqfactory, expr, type)

	# NB: I used to have clever code here that, given BOOLEAN
	# true- and false-case values, would compute the condition in boolean
	# space to avoid a transition into and out of arithmetic-land.
	# Then I realized that was stupid: the transition in (join) is free,
	# and the transition out a split, so the total cost is 2+k+1 muls.
	# But doing the condition in boolean space is 2k muls!

	def natural_type(self):
		return ARITHMETIC_TYPE

	def _reqcond(self):
		return LogicalCastReq(self.reqfactory, self.expr.cond, BOOLEAN_TYPE)

	def _reqtrue(self):
		return self.reqfactory.make_req(self.expr.valtrue, ARITHMETIC_TYPE)

	def _reqfalse(self):
		return self.reqfactory.make_req(self.expr.valfalse, ARITHMETIC_TYPE)

	def natural_dependencies(self):
		return [ self._reqcond(), self._reqtrue(), self._reqfalse() ]

	def natural_impl(self):
		buscond = self.get_bus_from_req(self._reqcond())
		bustrue = self.get_bus_from_req(self._reqtrue())
		busfalse = self.get_bus_from_req(self._reqfalse())
		return ArithmeticConditionalBus(
			self.board(), buscond, bustrue, busfalse)

##############################################################################
# Comparison operators.
# accept arithmetic inputs; emit boolean outputs
##############################################################################

class CmpReq(BinaryOpReq):
	def __init__(self, reqfactory, expr, type):
		BinaryOpReq.__init__(self, reqfactory, expr, type)
		# if (isinstance(expr, DFG.CmpLT)):
		# 	self._leq = False
		# elif (isinstance(expr, DFG.CmpLEQ)):
		# 	self._leq = True
		# else:
		# 	assert(False)

	def natural_type(self): return ARITHMETIC_TYPE

	def has_const_opt(self): return False	# doesn't cost muls.

        def var_impl(self, abus, bbus):
                assert(False)

class CmpLTReq(CmpReq):
        def __init__(self, reqfactory, expr, type):
                CmpReq.__init__(self, reqfactory, expr, type)

        def var_impl(self, abus, bbus):
                minusb_bus = ConstantMultiplyBus(self.board(), self.board().bit_width.get_neg1(), bbus)
                self.reqfactory.add_extra_bus(minusb_bus)

                comment = "CmpLT %s - %s" % (self.expr.left.__class__, self.expr.right.__class__)
                aminusb_bus = ArithAddBus(self.board(), comment, abus, minusb_bus)
                self.reqfactory.add_extra_bus(aminusb_bus)
                split_bus = SplitBus(self.board(), aminusb_bus)
                self.reqfactory.add_extra_bus(split_bus)
                signbit = LeftShiftBus(self.board(), split_bus, -self.board().bit_width.get_sign_bit())
                self.reqfactory.add_extra_bus(signbit)
                return JoinBus(self.board(), signbit)

class CmpLEQReq(CmpLTReq):
        """I made this a subclass of CmpLTReq because the logic is nearly identical."""
        def __init__(self, reqfactory, expr, type):
                CmpLTReq.__init__(self, reqfactory, expr, type)

        def var_impl(self, abus, bbus):
                constant_one = ConstantArithmeticBus(self.board(), 1)
                self.reqfactory.add_extra_bus(constant_one)
                comment = "CmpLEQ %s + 1" % (self.expr.right.__class__)
                bplus1_bus = ArithAddBus(self.board(), comment, bbus, constant_one)
                self.reqfactory.add_extra_bus(bplus1_bus)
                return  CmpLTReq.var_impl(self, abus, bplus1_bus)

class CmpEQReq(CmpReq):
        """An == operation.  This will have two possible implementations:  one using boolean operations, one using the compare-with-zero gate."""
        def __init__(self, reqfactory, expr, type):
                CmpReq.__init__(self, reqfactory, expr, type)

class CmpEQReqArith(CmpEQReq):
        def __init__(self, reqfactory, expr, type):
                CmpEQReq.__init__(self, reqfactory, expr, type)

        def var_impl(self, abus, bbus):
                """Perform equality test by subtracting and use the zerop gate"""
                # neg_bbus = ConstantMultiplyBus(self.board(), self.board().bit_width.get_neg1(), bbus)
                # self.reqfactory.add_extra_bus(neg_bbus)

                # aminusb_bus = ArithAddBus(self.board(), "", abus, neg_bbus)
                # self.reqfactory.add_extra_bus(aminusb_bus)

                # print aminusb_bus

                zerop_bus = ArithmeticZeroPBus(self.board(), abus, bbus)
                self.reqfactory.add_extra_bus(zerop_bus)

                assert(zerop_bus.get_trace_count() == 1)

                return zerop_bus

class CmpEQReqBoolean(CmpEQReq):
        def __init__(self, reqfactory, expr, type):
                CmpEQReq.__init__(self, reqfactory, expr, type)

        def var_impl(self, abus, bbus):
                """Perform equality test by subtracting, splitting, and checking for each bit being zero using a chain of ANDs"""
                neg1_bus = ConstantArithmeticBus(self.board(), -1)
                self.reqfactory.add_extra_bus(neg1_bus)

                neg_bbus = ArithMultiplyBus(self.board(), neg1, bbus)
                self.reqfactory.add_extra_bus(neg_bbus)

                aminusb_bus = ArithAddBus(self.board(), "", abus, neg_bbus)
                self.reqfactory.add_extra_bus(aminusb_bus)
                split_bus = SplitBus(self.board(), aminusb_bus)
                self.reqfactory.add_extra_bus(split_bus)

                # This next section is basically a map/fold pattern
                inv_bus = self.reqfactory.get_ConstantBitXorBus_class()(self.board(),
                                                                        self.board().bit_width.get_neg1(), split_bus)
                self.reqfactory.add_extra_bus(inv_bus)

                log_inv_bus = self.reqfactory.get_AllOnesBus_class()(self.board(), inv_bus)
                self.reqfactory.add_extra_bus(log_inv_bus)
                return JoinBus(self.board(), log_inv_bus)

	# def var_impl(self, abus, bbus):
# 		# a <  b :: a+(neg1*b) < 0 :: sign_bit(a+(neg1*b))
# 		# for integers, a <= b :: a < b+1 :: sign_bit(a+(neg1*(b+1)))
# 		# TODO optimizations for comparisons.
# 		# Once again, there's enough complexity here that we might imagine
# 		# we could collapse common subexpressions at a lower level than we
# 		# are, since sign_bit is pretty expensive.
# 		# That way, if (a<b) { return a-b; } would be more efficient.
# 		# Another optimization alternative is to only use only as many bits
# 		# of neg1 as we need to compute a-b.
# 		# For now, let's do this all in 32-bits, and collapse no redundancy.
# 		if (self._leq):
# 			constant_one = ConstantArithmeticBus(self.board(), 1)
# 			self.reqfactory.add_extra_bus(constant_one)
# 			comment = "Cmp::%s + 1" % (self.expr.right.__class__,)
# 			bplus1_bus = ArithAddBus(self.board(), comment, bbus, constant_one)
# 			self.reqfactory.add_extra_bus(bplus1_bus)
# 			adjusted_bbus = bplus1_bus
# 		else:
# 			adjusted_bbus = bbus
# #		minusb_bus = ConstantMultiplyBus(self.board(), -1, adjusted_bbus)
# 		minusb_bus = ConstantMultiplyBus(self.board(), self.board().bit_width.get_neg1(), adjusted_bbus)
# 		self.reqfactory.add_extra_bus(minusb_bus)
# 		comment = "Cmp::%s - %s" % (
# 			self.expr.left.__class__,
# 			self.expr.right.__class__)
# 		aminusb_bus = ArithAddBus(self.board(), comment, abus, minusb_bus)
# 		self.reqfactory.add_extra_bus(aminusb_bus)
# 		split_bus = SplitBus(self.board(), aminusb_bus)
# 		self.reqfactory.add_extra_bus(split_bus)
# 		signbit = LeftShiftBus(
# 			self.board(), split_bus, -self.board().bit_width.get_sign_bit())
# 		self.reqfactory.add_extra_bus(signbit)
# 		# NB we join the one-bit sign bit back into an ARITHMETIC bus
# 		# so we can keep our input and output types equivalent (ARITHMETIC),
# 		# and hence exploit the BinaryOpReq superclass. We could defect --
# 		# at a cost in more complexity -- but there's
# 		# no point. The join is free, and if someone else needs a boolean,
# 		# well, the one-bit split is free, too.
# 		return JoinBus(self.board(), signbit)


