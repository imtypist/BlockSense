class DFGOperator: pass

class PyOp(DFGOperator):
	def __init__(self, pyop):
		self.pyop = pyop

	def __call__(self, *args):
		return self.pyop(*args)

class LeftShiftOp(DFGOperator):
	def __init__(self, bw):
		self.bw = bw

	def __call__(self, a, b):
		return self.bw.leftshift(a, b)

class RightShiftOp(DFGOperator):
	def __init__(self, bw):
		self.bw = bw

	def __call__(self, a, b):
		return self.bw.rightshift(a, b)

class LogicalAndOp(DFGOperator):
	def __call__(self, a, b):
		return a and b
