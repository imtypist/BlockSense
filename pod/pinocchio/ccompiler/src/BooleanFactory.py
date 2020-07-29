from ReqFactory import ReqFactory
from TraceType import BOOLEAN_TYPE
from DFG import *
from BooleanBusReq import *

class BooleanFactory(ReqFactory):
	def __init__(self, output_filename, circuit_inputs, circuit_outputs, bit_width):
		ReqFactory.__init__(self, output_filename, circuit_inputs, circuit_outputs, bit_width)

	def type(self):
		return BOOLEAN_TYPE

	def make_zero_bus(self):
		return BooleanZero(self.get_board())

	def make_input_req(self, expr):
		return BooleanInputReq(self, expr, self.type())

	def make_output_bus(self, expr_bus, idx):
		return BooleanOutputBus(self.get_board(), expr_bus, idx)

	def make_req(self, expr, type):
		assert(type==BOOLEAN_TYPE)
		if (isinstance(expr, Input)):
			result = BooleanInputReq(self, expr, type)
		elif (isinstance(expr, Conditional)):
			result = ConditionalReq(self, expr, type)
		elif (isinstance(expr, CmpLT)):
			result = CmpReq(self, expr, type)
		elif (isinstance(expr, CmpLEQ)):
			result = CmpReq(self, expr, type)
		elif (isinstance(expr, Constant)):
			result = ConstantReq(self, expr, type)
		elif (isinstance(expr, Add)):
			result = AddReq(self, expr, type)
		elif (isinstance(expr, Subtract)):
			result = SubtractReq(self, expr, type)
		elif (isinstance(expr, Multiply)):
			result = MultiplyReq(self, expr, type)
		elif (isinstance(expr, Negate)):
			result = NegateReq(self, expr, type)
		else:
			result = ReqFactory.make_req(self, expr, type)
		return result

	def collapse_req(self, req):
		return req.natural_impl()

	def get_BitAndBus_class(self): return BitAndBus
	def get_BitOrBus_class(self): return BitOrBus
	def get_XorBus_class(self): return XorBus
	def get_ConstantBitXorBus_class(self): return ConstantBitXorBus
	def get_AllOnesBus_class(self): return AllOnesBus
