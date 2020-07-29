from BitWidth import *
from Buses import *

class Board:
	# :: board layer: abstracts into values that travel on busses made
	# of traces.
	# 	a computation produces a bus-of-traces, but may internally generate some
	# 	private wires to do the computation (see a|b).
	# Get it? Like a printed circuit board?
	def __init__(self, max_width, bit_width):
		self.max_width = max_width	# eg 254 (largest 2^k <= P)
		assert(isinstance(bit_width, BitWidth))
		self.bit_width = bit_width	# defines C int overflow, sign-bit behavior
		self.order_alloc = 0

		self._one_bus = OneBus(self)

	def get_one_bus(self):
		# called from ReqFactory init to register the special _one_bus in
		# the list of buses to emit.
		return self._one_bus

	def one_wire(self):
		return self._one_bus.get_one_wire()

	def set_zero_bus(self, zero_bus):
		self._zero_bus = zero_bus 

	def zero_wire(self):
		return self._zero_bus.get_trace(0)

	def assign_order(self):
		self.order_alloc += 1
		return self.order_alloc

	def is_board(self):
		# facilitates an assertion in a class that can't import this module
		# (because things would get recursive)
		return True
