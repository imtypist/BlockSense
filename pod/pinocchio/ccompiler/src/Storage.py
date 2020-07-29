##############################################################################

# Storage objects are *NOT* equivalent just because they have
# equal labels! When we call a subroutine that replaces the name 'x'
# with a new x, the new x 'allocates' new Storage, so that refs to
# x can point to it, without it aliasing the previous x.

class Storage:
	def __init__(self, label, size):
		self.label = label
		self.size = size

	def __hash__(self):
		return id(self)

	def __cmp__(self, other):
		return id(self)-id(other)

	def __repr__(self):
		return "storage(%s[%d]@%x)" % (self.label, self.size, id(self))
		#return "%s" % self.label

class Null(Storage):
	# somewhere for undefined pointers to point.
	def __init__(self):
		Storage.__init__(self, "null", 0)

	def __repr__(self):
		return "null"

##############################################################################

class Key: pass

class Symbol(Key):
	def __init__(self, name):
		self.name = name

	def __hash__(self):
		return hash(self.name)

	def __cmp__(self, other):
		i = cmp(self.__class__, other.__class__)
		if (i!=0): return i
		return cmp(self.name, other.name)

	def __repr__(self):
		return self.name

class PseudoSymbol(Symbol):
	def __init__(self, name):
		Symbol.__init__(self, name)
		# NB will fail (on __class__ test) to compare equal to an equivalent
		# Symbol actually defined in C file.

	def __repr__(self):
		return "~%s" % self.name

class StorageKey(Key):
	def __init__(self, storage, idx):
		self.storage = storage
		self.idx = idx

	def __repr__(self):
		#return "%s@%x[%s]" % (self.storage, id(self.storage), self.idx)
		return "%s[%s]" % (self.storage, self.idx)

	def __hash__(self):
		return hash(self.storage) ^ hash(self.idx)

	def __cmp__(self, other):
		i = cmp(self.__class__, other.__class__)
		if (i!=0): return i
		i = cmp(self.storage, other.storage)
		if (i!=0): return i
		return cmp(self.idx, other.idx)

