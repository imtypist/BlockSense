class TraceType:
	def __init__(self, name): self.name = name
	def __repr__(self): return self.name

#	def opposite(self):
#		if (self==ARITHMETIC_TYPE):
#			return BOOLEAN_TYPE
#		elif (self==BOOLEAN_TYPE):
#			return ARITHMETIC_TYPE
#		else:
#			assert(False)
# 

ARITHMETIC_TYPE = TraceType("A")
BOOLEAN_TYPE = TraceType("B")

