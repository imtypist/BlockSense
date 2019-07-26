import CircuitParams

class BitWidth:
	def __init__(self, width, ignore_overflow):
		self.width = width
		if (ignore_overflow):
			self.overflow_limit = None
		else:
			self.overflow_limit = CircuitParams.ACTIVE_BIT_CONSTRAINT

	def ignoring_overflow():
		return self.overflow_limit==None

	def get_width(self):
		return self.width

	def get_sign_bit(self):
		return self.width - 1

	def get_neg1(self):
		return (1<<self.width)-1

	def leftshift(self, a, b):
		return (a<<b) & self.get_neg1()

	def rightshift(self, a, b):
		return ((a & self.get_neg1()) >> b)
		# NB TODO rightshift is *always* unsigned in our world; we're not
		# really honoring the sign bit.

	def truncate(self, bits):
		if (self.overflow_limit != None
			and bits >= self.get_width()):
			return self.get_width()
		else:
			return bits

