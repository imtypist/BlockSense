import types

# This file defines the set of core field objects (Wires) and
# operations (FieldOps) available in the underlying encoding.
# We build higher-level functions up by creating bus objects that
# apply one or more FieldOps to groups of wires.

class Wire:
	def __init__(self, idx):
		assert(type(idx)==types.IntType)
		self.idx = idx

	def __repr__(self):
		return "%d" % self.idx
	
	def __hash__(self):
		return self.idx

	def __cmp__(self, other):
		return cmp(self.idx, other.idx)

class WireList:
	def __init__(self, wires):
		assert(type(wires)==types.ListType)
		self.wires = wires

	def __repr__(self):
		l = " ".join(map(repr, self.wires))
		return "%d <%s>" % (len(self.wires), l)

	def __len__(self):
		return len(self.wires)

	def __getitem__(self, j):
		return self.wires[j]
