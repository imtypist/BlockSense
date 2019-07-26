from ReqFactory import ReqFactory
from TraceType import ARITHMETIC_TYPE
from DFG import *
from ArithBusReq import *

class ArithFactory(ReqFactory):
	def __init__(self, output_filename, circuit_inputs, circuit_nizk_inputs, circuit_outputs, bit_width):
		ReqFactory.__init__(self, output_filename, circuit_inputs, circuit_nizk_inputs, circuit_outputs, bit_width)

	def type(self):
		return ARITHMETIC_TYPE

	def make_zero_bus(self):
		return ArithZero(self.get_board())

	def make_input_req(self, expr):
		return ArithmeticInputReq(self, expr, self.type())

	def make_nizk_input_req(self, expr):
		return ArithmeticNIZKInputReq(self, expr, self.type())

	def make_output_bus(self, expr_bus, idx):
		return ArithmeticOutputBus(self.get_board(), expr_bus, idx)

	def make_req(self, expr, type):
		if (isinstance(expr, Input)):
			result = ArithmeticInputReq(self, expr, type)
		elif (isinstance(expr, NIZKInput)):
			result = ArithmeticNIZKInputReq(self, expr, type)
		elif (isinstance(expr, Conditional)):
			result = ConditionalReq(self, expr, type)
		elif (isinstance(expr, CmpLT)):
			result = CmpLTReq(self, expr, type)
		elif (isinstance(expr, CmpLEQ)):
			result = CmpLEQReq(self, expr, type)
                elif (isinstance(expr, CmpEQ)):
                        result = CmpEQReqArith(self, expr, type)
		elif (isinstance(expr, Constant)):
			result = ConstantReq(self, expr, type)
		elif (isinstance(expr, Add)):
			result = AddReq(self, expr, type)
		elif (isinstance(expr, Subtract)):
			# NB trying something new here. expr factory does memoization
			# against existing graph, so rolling up a late expr ensures
			# we'll avoid generating a duplicate Negate. (Not that negate
			# costs any gates; eyeroll.)
			neg_expr = expr.factory.create(Negate, expr.right)
			add_expr = expr.factory.create(Add, expr.left, neg_expr)
			result = AddReq(self, add_expr, type)
		elif (isinstance(expr, Multiply)):
			result = MultiplyReq(self, expr, type)
		elif (isinstance(expr, Negate)):
			result = NegateReq(self, expr, type)
		else:
			result = ReqFactory.make_req(self, expr, type)
		return result

	def collapse_req(self, req):
#		print "requested %s and %s is naturally %s" % (
#			req.type, req, req.natural_type())
		if (req.natural_type() == req.type):
			return req.natural_impl()
		else:
			bus = req.get_bus_from_req(req._natural_req())
			if (req.type==BOOLEAN_TYPE):
				return SplitBus(req.board(), bus)
			elif (req.type==ARITHMETIC_TYPE):
				return JoinBus(req.board(), bus)
			else: assert(False)

	# I'm doing something very polymorphic, a little first-order,
	# and definitely unreadable here. Wish I could think of an elegant
	# way to express this.
	def get_BitAndBus_class(self): return BitAndBus
	def get_BitOrBus_class(self): return BitOrBus
	def get_XorBus_class(self): return XorBus
	def get_ConstantArithmeticBus_class(self): return ConstantArithmeticBus
	def get_ConstantBitXorBus_class(self): return ConstantBitXorBus
	def get_AllOnesBus_class(self): return AllOnesBus
        def get_EqlBus_class(self): return EqlBusArith

	# truncation cache
	def truncate(self, expr, bus):
		if (expr in self.truncated_bus_cache):
#			print "truncated_bus_cache just recycled a split!"
			return self.truncated_bus_cache[expr]
		self.add_extra_bus(bus)
		truncated_bool_bus = SplitBus(self.get_board(), bus)
		self.add_extra_bus(truncated_bool_bus)
		truncated_bus = JoinBus(self.get_board(), truncated_bool_bus)
		self.truncated_bus_cache[expr] = truncated_bus
		return truncated_bus

