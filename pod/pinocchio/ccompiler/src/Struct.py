import operator

class NoSuchStructField(Exception):
	def __init__(self, s): self.s = s
	def __str__(self): return "NoSuchStructField(%s)" % self.s
	def __repr__(self): return "NoSuchStructField(%s)" % self.s

class PointersCannotBeValues(Exception): pass

class Field:
	def __init__(self, name, type):
		self.name = name
		self.type = type

	def __repr__(self):
		return "%s %s" % (self.type, self.name)

class Type: pass

class IntType(Type):
	def sizeof(self):
		return 1

	def __repr__(self):
		return "int";

	def __cmp__(self, other):
		return cmp(self.__class__, other.__class__)

	def is_ptr_type(self):
		return False

class UnsignedType(IntType):
	pass

class ArrayType(Type):
	def __init__(self, type, size):
		self.type = type
		self.size = size
		assert(self.size!=None)
	
	def sizeof(self):
		return self.size * self.type.sizeof()

	def __repr__(self):
		return "%s[%s]" % (self.type, self.size)

	def __cmp__(self, other):
		i = cmp(self.__class__, other.__class__)
		if (i!=0): return i
		i = cmp(self.type, other.type)
		if (i!=0): return i
		return cmp(self.size, other.size)

	def is_ptr_type(self):
		return True

class StructType(Type):
	def __init__(self, label, fields):
		self.label = label
		self.fields = fields
		self.offsets = [0]*(len(self.fields)+1)
		#print "fields %s" % self.fields
		for i in range(len(self.offsets)-1):
			#print "struct %s field %s type %s" % (self.label, i, self.fields[i].type)
			self.offsets[i+1] = self.offsets[i] + self.fields[i].type.sizeof()
		#print "struct %s offsets: %s" % (self.label, self.offsets)

	def sizeof(self):
		return self.offsets[-1]

	def indexof(self, name):
		for i in range(len(self.fields)):
			if (self.fields[i].name == name):
				return i
		raise NoSuchStructField(name)
		
	def offsetof(self, name):
		return self.offsets[self.indexof(name)]

	def get_field(self, name):
		return self.fields[self.indexof(name)]

	def __repr__(self):
		return "struct %s" % self.label

	def __cmp__(self, other):
		i = cmp(self.__class__, other.__class__)
		if (i!=0): return i
		return cmp(self.label, other.label)

	def is_ptr_type(self):
		return False

class PtrType(Type):
	def __init__(self, type):
		self.type = type

	def sizeof(self):
		raise PointersCannotBeValues()

	def __repr__(self):
		return "*%s" % self.type

	def __cmp__(self, other):
		i = cmp(self.__class__, other.__class__)
		if (i!=0): return i
		return cmp(self.type, other.type)

	def is_ptr_type(self):
		return True
