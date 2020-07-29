from Buses import *
from TraceType import *
import DFG

class BaseReq:
	def __init__(self, reqfactory, expr, type):
		assert(reqfactory.is_req_factory())
#		assert(isinstance(expr, DFGExpr))
		assert(isinstance(type, TraceType))
		self.reqfactory = reqfactory
		self.expr = expr
		self.type = type
		# This is the entire object description; subclasses
		# may not add additional state.
		# That lets me do the type-switching code here, and
		# lets us do object equality (for collapsing) here rather
		# than with another ugly memoization scheme.
	
	def _tuple(self):
		return (self.expr, self.type)

	def __hash__(self):
		return hash(self._tuple())

	def __cmp__(self, other):
		return cmp(self._tuple(), other._tuple())

	def __repr__(self):
		return "%s:%s" % (self.__class__, self.type)

	def make_req(self, expr, type):
		return self.reqfactory.make_req(expr, type)

	def get_bus_from_req(self, req):
		return self.reqfactory.collapser().lookup(req)

	def get_bus(self, expr, type):
		return self.get_bus_from_req(self.make_req(expr, type))

	def board(self):
		return self.reqfactory.get_board()

class BusReq(BaseReq):
	def __init__(self, reqfactory, expr, type):
		BaseReq.__init__(self, reqfactory, expr, type)

	# subclass defines what happens when request is for the type
	# that the operator generates naturally.
	def natural_type(self):
		raise Exception("abstract method")

	def natural_dependencies(self):
		raise Exception("abstract method in %s" % self.__class__)

	def natural_impl(self):
		raise Exception("abstract method")

	# we define what happens when the request is for the opposite type
	def _natural_req(self):
		return self.__class__(self.reqfactory, self.expr, self.natural_type())

	def get_dependencies(self):
		if (self.natural_type() != self.type):
			return [self._natural_req()]
		else:
			return self.natural_dependencies()

	def collapse_impl(self):
		return self.reqfactory.collapse_req(self)

class BinaryOpReq(BusReq):
	# A binary op knows it has two input arguments (and hence knows
	# how to specify them as dependencies and find their busses later),
	# and can also detect and convert the operation into an optimized
	# version when one input is a constant expression.
	def __init__(self, reqfactory, expr, type):
		BusReq.__init__(self, reqfactory, expr, type)

		if (self.has_const_opt() and isinstance(expr.left, DFG.Constant)):
			self.is_const_opn = True
			self._const_expr = expr.left
			self.variable_expr = expr.right
		elif (self.has_const_opt() and isinstance(expr.right, DFG.Constant)):
			self.is_const_opn = True
			self._const_expr = expr.right
			self.variable_expr = expr.left
		else:
			# deult BinaryOpReq behavior
			self.is_const_opn = False

	def __repr__(self):
		return "%s(%s,%s)" % (
			self.__class__,
			self.expr.left.__class__,
			self.expr.right.__class__,)

	# return true if subclass implements const_impl
	def has_constant_opt(self):
		raise Exception("abstract method")

	# return appropriate bus for the constant optimization case
	# NB this assumes the operation is commutative, since subclass
	# impl receives no argument order information in this case.
	def const_impl(self, const_expr, var_bus):
		raise Exception("abstract method")

	# return appropriate bus for the case where both args are variable
	def var_impl(self, var_bus):
		raise Exception("abstract method")

	def _variable_req(self):
		return self.make_req(self.variable_expr, self.natural_type())

	def natural_dependencies(self):
		if (self.is_const_opn):
			return [ self._variable_req() ]
		else:
			return [
				self.make_req(self.expr.left, self.natural_type()),
				self.make_req(self.expr.right, self.natural_type())
				];

	def _core_impl(self, transform):
		if (self.is_const_opn):
			var_bus = self.get_bus_from_req(self._variable_req())
			return self.const_impl(self._const_expr, transform(var_bus))
		else:
			busses = [
				transform(self.get_bus(self.expr.left, self.natural_type())),
				transform(self.get_bus(self.expr.right, self.natural_type()))
				]
			for bus in busses:
				self.reqfactory.add_extra_bus(bus)
			return self.var_impl(*busses)

	def natural_impl(self):
		def identity(bus):
			return bus

		def truncate(bus):
			# Truncations aren't memoized in the collapser,
			# because this is the only place we'd try to sneak them back
			# out.
			return self.reqfactory.truncate(self.expr, bus)

		bus = self._core_impl(identity)

		overflow_limit = self.board().bit_width.overflow_limit
		if (overflow_limit!=None
			and bus.get_active_bits() > overflow_limit):
			# NB in this case we end up discarding the bus object
			# we made, but that's okay, because we'll recreate it later.
			# And it doesn't even get added into the bus set.
			truncated_bus = self._core_impl(truncate)
#			print "Truncating %d-bit term to %d bits" % (
#				bus.get_active_bits(), truncated_bus.get_active_bits())
			# TODO store
			bus = truncated_bus
		return bus

##############################################################################
# Boolean operators
##############################################################################

class NotFamily(BusReq):
	def __init__(self, reqfactory, expr, type):
		BusReq.__init__(self, reqfactory, expr, type)

	def make_bitnot(self, bus):
		return self.reqfactory.get_ConstantBitXorBus_class()(
			self.board(), self.board().bit_width.get_neg1(), bus)

	def make_logical_not(self, bus):
		bitnot = self.make_bitnot(bus)
		self.reqfactory.add_extra_bus(bitnot)
		return self.reqfactory.get_AllOnesBus_class()(self.board(), bitnot)

	def make_logical_cast(self, bus):
		logical_not = self.make_logical_not(bus)
		self.reqfactory.add_extra_bus(logical_not)
		return make_bitnot(logical_not)

class BitNotReq(NotFamily):
	def __init__(self, reqfactory, expr, type):
		NotFamily.__init__(self, reqfactory, expr, type)

	def natural_type(self): return BOOLEAN_TYPE

	def _req(self):
		return self.make_req(self.expr.expr, BOOLEAN_TYPE)

	def natural_dependencies(self):
		return [ self._req() ]

	def natural_impl(self):
		return self.make_bitnot(self.get_bus_from_req(self._req()))

class LogicalNotReq(NotFamily):
	def __init__(self, reqfactory, expr, type):
		NotFamily.__init__(self, reqfactory, expr, type)

	def natural_type(self): return BOOLEAN_TYPE

	def _req(self):
		return self.make_req(self.expr.expr, BOOLEAN_TYPE)

	def natural_dependencies(self):
		return [ self._req() ]

	def natural_impl(self):
		return self.make_logical_not(self.get_bus_from_req(self._req()))

class LogicalCastReq(NotFamily):
	# cast a wide value (perhaps one that came as arithmetic) down to
	# a one-bit boolean truth value by testing all of its bits.
	def __init__(self, reqfactory, expr, type):
		NotFamily.__init__(self, reqfactory, expr, type)

	def natural_type(self): return BOOLEAN_TYPE

	def _req(self):
		return self.make_req(self.expr, BOOLEAN_TYPE)

	def natural_dependencies(self):
		return [ self._req() ]

	def natural_impl(self):
		wide_bus = self.get_bus_from_req(self._req())
		if (wide_bus.get_active_bits()==1):
			print "LogicalCastReq was cheap"
			return BooleanCastBus(wide_bus)
		else:
			print "LogicalCastReq was expensive, from %s" % wide_bus.get_active_bits()
			return self.make_logical_cast(wide_bus)

class ShiftReq(BusReq):
	def __init__(self, reqfactory, expr, type):
		BusReq.__init__(self, reqfactory, expr, type)
		assert(isinstance(self.expr.right, DFG.Constant))

	def natural_type(self): return BOOLEAN_TYPE

	def _req(self):
		return self.make_req(self.expr.left, BOOLEAN_TYPE)

	def natural_dependencies(self):
		return [ self._req() ]

	def natural_impl(self):
		return LeftShiftBus(
			self.board(),
			self.get_bus_from_req(self._req()),
			self.sign() * self.expr.right.value)

	def sign(self):
		raise Exception("abstract method")

class LeftShiftReq(ShiftReq):
	def __init__(self, reqfactory, expr, type):
		ShiftReq.__init__(self, reqfactory, expr, type)
	
	def sign(self): return 1

class RightShiftReq(ShiftReq):
	def __init__(self, reqfactory, expr, type):
		ShiftReq.__init__(self, reqfactory, expr, type)
	
	def sign(self): return -1


class BooleanBinaryReq(BinaryOpReq):
	def __init__(self, reqfactory, expr, type, cls):
		BinaryOpReq.__init__(self, reqfactory, expr, type)
		self._cls = cls

	def natural_type(self): return BOOLEAN_TYPE

	def has_const_opt(self): return False

	def var_impl(self, *busses):
		return self._cls(self.board(), *busses)

class BitAndReq(BooleanBinaryReq):
	def __init__(self, reqfactory, expr, type):
		BooleanBinaryReq.__init__(self, reqfactory, expr, type, reqfactory.get_BitAndBus_class())

	def has_const_opt(self): return True

	def const_impl(self, const_expr, variable_bus):
		return ConstBitAndBus(self.board(), const_expr.value, variable_bus)

class BitOrReq(BooleanBinaryReq):
	def __init__(self, reqfactory, expr, type):
		BooleanBinaryReq.__init__(self, reqfactory, expr, type, reqfactory.get_BitOrBus_class())

	def has_const_opt(self): return True

	def const_impl(self, const_expr, variable_bus):
		return ConstBitOrBus(self.board(), const_expr.value, variable_bus)

class XorReq(BooleanBinaryReq):
	def __init__(self, reqfactory, expr, type):
		BooleanBinaryReq.__init__(self, reqfactory, expr, type, reqfactory.get_XorBus_class())

	def has_const_opt(self): return True

	def const_impl(self, const_expr, variable_bus):
		return self.reqfactory.get_ConstantBitXorBus_class()(
			self.board(), const_expr.value, variable_bus)

############################################################################## 
# The ConstantReq can supply a value in either type, so it skips the
# BusReq machinery that automatically translates output types.
############################################################################## 

class ConstantReq(BaseReq):
	def __init__(self, reqfactory, expr, type):
		BaseReq.__init__(self, reqfactory, expr, type)

	def get_dependencies(self):
		return []

	def collapse_impl(self):
		# NB we could "optimize", when value==1, by not emitting
		# a const-mul-1(one_wire), but that doesn't actually cost anything
		# other than bloat in the arith file; it disappears in the
		# polynomial representation. So meh.
		if (self.type==BOOLEAN_TYPE):
			# a single wire, which can be added into other wires.
			# (muls should be clever enough to skip this step.)
			return ConstantBooleanBus(self.board(), self.expr.value)
		elif (self.type==ARITHMETIC_TYPE):
			# ones and zeros
			return (self.reqfactory.get_ConstantArithmeticBus_class()
				(self.board(), self.expr.value))
		else: assert(False)
