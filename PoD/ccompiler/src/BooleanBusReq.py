from Buses import *
from BooleanBuses import *
from TraceType import *
import DFG
from BusReq import *

##############################################################################
# Boolean-only variants of arithmetic-like operators
##############################################################################

class BooleanReq(BusReq):
	def __init__(self, reqfactory, expr, type):
		BusReq.__init__(self, reqfactory, expr, type)

	def natural_type(self): return BOOLEAN_TYPE

class BooleanInputReq(BooleanReq):
	def __init__(self, reqfactory, expr, type):
		BooleanReq.__init__(self, reqfactory, expr, type)

	def natural_dependencies(self):
		return []

	def natural_impl(self):
		return BooleanInputBus(self.board(), self.expr.storage_key.idx)

# I'd double-inherit here, but that's insane. So we'll duplicate
# the behavior of BooleanReq
class BooleanBinaryOpReq(BinaryOpReq):
	def __init__(self, reqfactory, expr, type):
		BinaryOpReq.__init__(self, reqfactory, expr, type)

	def natural_type(self): return BOOLEAN_TYPE

class AddReq(BooleanBinaryOpReq):
	def __init__(self, reqfactory, expr, type):
		BooleanBinaryOpReq.__init__(self, reqfactory, expr, type)

	def natural_type(self): return BOOLEAN_TYPE

	def has_const_opt(self): return False
		# TODO there may be a constant optimization here

	def var_impl(self, *busses):
		return BooleanAddBus(self.board(), *busses)

class MultiplyReq(BooleanBinaryOpReq):
	def __init__(self, reqfactory, expr, type):
		BooleanBinaryOpReq.__init__(self, reqfactory, expr, type)

	def natural_type(self): return BOOLEAN_TYPE

	def has_const_opt(self): return False
		# TODO almost certainly a constant optimization here

	def var_impl(self, *busses):
		return BooleanMultiplyBus(self.board(), *busses)

class ConditionalReq(BooleanReq):
	def __init__(self, reqfactory, expr, type):
		BooleanReq.__init__(self, reqfactory, expr, type)

	# NB: I used to have clever code here that, given BOOLEAN
	# true- and false-case values, would compute the condition in boolean
	# space to avoid a transition into and out of arithmetic-land.
	# Then I realized that was stupid: the transition in (join) is free,
	# and the transition out a split, so the total cost is 2+k+1 muls.
	# But doing the condition in boolean space is 2k muls!

	def natural_type(self):
		return BOOLEAN_TYPE

	def _reqcond(self):
		return LogicalCastReq(self.reqfactory, self.expr.cond, BOOLEAN_TYPE)

	def _reqtrue(self):
		return self.reqfactory.make_req(self.expr.valtrue, BOOLEAN_TYPE)

	def _reqfalse(self):
		return self.reqfactory.make_req(self.expr.valfalse, BOOLEAN_TYPE)

	def natural_dependencies(self):
		return [ self._reqcond(), self._reqtrue(), self._reqfalse() ]

	def natural_impl(self):
		buscond = self.get_bus_from_req(self._reqcond())
		bustrue = self.get_bus_from_req(self._reqtrue())
		busfalse = self.get_bus_from_req(self._reqfalse())
		return BooleanConditionalBus(
			self.board(), buscond, bustrue, busfalse)

##############################################################################
# Comparison operators.
# accept arithmetic inputs; emit boolean outputs
##############################################################################

class CmpReq(BooleanBinaryOpReq):
	def __init__(self, reqfactory, expr, type):
		BooleanBinaryOpReq.__init__(self, reqfactory, expr, type)
		if (isinstance(expr, DFG.CmpLT)):
			self._leq = False
		elif (isinstance(expr, DFG.CmpLEQ)):
			self._leq = True
		else:
			assert(False)

	def natural_type(self): return BOOLEAN_TYPE

	def has_const_opt(self): return False
		# TODO there may be a constant optimization here

	def var_impl(self, abus, bbus):
		# TODO same styles of optimizations for arithmetic optimizations
		# apply here, too.
		# See KSS sec 3.2
		TODO
