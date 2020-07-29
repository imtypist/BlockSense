import types
from Storage import *
from Struct import *
from BitWidth import *
from DFGOperator import *

class NonconstantArrayAccess(Exception):
	def __init__(self, s):
		Exception.__init__(self, s)

class NonconstantExpression(Exception): pass

class UndefinedExpression(Exception): pass

class Total: pass
total = Total()
total.count = 0
total.flatbytes = 0
total.flats = []

class DFGFactory:
	def __init__(self):
		self.intern = {}

	def create(self, dclass, *args):
		def uid(obj):
			#return id(obj)
			if (type(obj)==types.ClassType
				or type(obj)==types.InstanceType):
				return id(obj)
			elif (type(obj)==types.IntType
				or type(obj)==types.LongType
				or type(obj)==types.BooleanType):
				return obj
			else:
				raise Exception("unhandled arg type %s" % type(obj))
		internid = tuple(map(uid, (dclass,)+args))
		if (internid in self.intern):
			return self.intern[internid]
		obj = dclass(self, *args)
		self.intern[internid] = obj
		return obj

	def __getstate__(self):
		return None

	def __setstate__(self, state):
		pass

	def verify(self):
		# convince myself that all creations are being interned through the
		# factory.
		pass

class DFGExpr:
	def __init__(self, factory):
		factory.verify()
		self.factory = factory

	def __cmp__(self, other):
		i=cmp(type(self), type(other))
		if (i!=0): return i
		return cmp(id(self), id(other))

	def __hash__(self):
		return id(self)

class InputBase(DFGExpr):
	def __init__(self, factory, storage_key):
		DFGExpr.__init__(self, factory)
		self.storage_key = storage_key

	def __repr__(self):
		return "(%s %s)" % (self.__class__.__name__, self.storage_key)

	def evaluate(self, collapser):
		return collapser.get_input(self.storage_key)

	def collapse_dependencies(self):
		return []

	def collapse_constants(self, collapser):
		return self

	def __hash__(self):
		return hash(self.storage_key)

	def equal(self, other):
		i = cmp(id(self), id(other))
		if (i==0): return i
		i = cmp(self.__class__, other.__class__)
		if (i!=0): return i
		i = cmp(self.storage_key, other.storage_key)
		return i

class Input(InputBase):
	def __init__(self, factory, storage_key):
		InputBase.__init__(self, factory, storage_key)

class NIZKInput(InputBase):
	def __init__(self, factory, storage_key):
		InputBase.__init__(self, factory, storage_key)

class Undefined(DFGExpr):
	def __init__(self, factory):
		DFGExpr.__init__(self, factory)

	def __repr__(self):
		return "Undefined"

	def evaluate(self, collapser):
		raise UndefinedExpression()

	def collapse_dependencies(self):
		return []

	def collapse_constants(self, collapser):
		return self

class Constant(DFGExpr):
	def __init__(self, factory, value):
		DFGExpr.__init__(self, factory)
		self.value = value

	def __repr__(self):
		return "(C %d)" % self.value

	def evaluate(self, collapser):
		return self.value

	def collapse_dependencies(self):
		return []

	def collapse_constants(self, collapser):
		return self

	def __hash__(self):
		return hash(self.value)

	def equal(self, other):
		i = cmp(self.__class__, other.__class__)
		if (i!=0): return i
		i = cmp(self.value, other.value)
		return i

class Op(DFGExpr):
	pass

class BinaryOp(Op):
	def __init__(self, factory, op, left, right):
		DFGExpr.__init__(self, factory)
		self.op = op
		self.left = left
		self.right = right
		assert(self.left!=None)
		assert(self.right!=None)
		assert(not isinstance(self.left, StorageRef))
		assert(not isinstance(self.right, StorageRef))
		self.memo_repr = None
	
	def __repr__(self):
#		if (self.memo_repr == None):
#			self.memo_repr = "(%s %s %s)" % (self.op, self.left, self.right)
#		return self.memo_repr
		return "(%s %s %s)" % (self.op, self.left, self.right)

	def equal(self, other):
		i = cmp(id(self), id(other))
		if (i==0): return i
		i = cmp(self.__class__, other.__class__)
		if (i!=0): return i
		i = cmp(self.op, other.op)
		if (i!=0): return i
		i = cmp(self.left, other.left)
		if (i!=0): return i
		i = cmp(self.right, other.right)
		return i

	def extra_create_args(self):
		return []

	def collapse_dependencies(self):
		return [self.left, self.right]

	def collapse_constants(self, collapser):
		collapsed = self.factory.create(
			self.__class__,
			collapser.lookup(self.left),
			collapser.lookup(self.right),
			*self.extra_create_args())

		# and can we collapse away this operator, too?
		try:
			evaluated = collapser.evaluate_as_constant(collapsed)
			collapsed = self.factory.create(Constant, evaluated)
		except NonconstantExpression:
			pass

		return collapsed

class BinaryMath(BinaryOp):
	def __init__(self, factory, op, pyop, identity, left, right):
		BinaryOp.__init__(self, factory, op, left, right)
		self.pyop = pyop
		self.identity = identity

	def evaluate(self, collapser):
		return self.pyop(collapser.lookup(self.left), collapser.lookup(self.right))

	def collapse_dependencies(self):
		return [self.left, self.right]

	def collapse_constants(self, collapser):
		basic = BinaryOp.collapse_constants(self, collapser)
		if (isinstance(basic, self.__class__)):
			if (isinstance(basic.left, Constant) and basic.left.value==self.identity):
				return basic.right
			elif (isinstance(basic.right, Constant) and basic.right.value==self.identity):
				return basic.left
		return basic


class Add(BinaryMath):
	def __init__(self, f, left, right):
		BinaryMath.__init__(self, f, "+", PyOp(operator.add), 0, left, right)

class Subtract(BinaryMath):
	def __init__(self, f, left, right):
		BinaryMath.__init__(self, f, "-", PyOp(operator.sub), 0, left, right)

class Multiply(BinaryMath):
	def __init__(self, f, left, right):
		BinaryMath.__init__(self, f, "*", PyOp(operator.mul), 1, left, right)

class Divide(BinaryMath):
	def __init__(self, f, left, right):
		BinaryMath.__init__(self, f, "/", PyOp(operator.div), None, left, right)

class Modulo(BinaryMath):
	def __init__(self, f, left, right):
		BinaryMath.__init__(self, f, "%", PyOp(operator.mod), None, left, right)

class Xor(BinaryMath):
	def __init__(self, f, left, right):
		BinaryMath.__init__(self, f, "^", PyOp(operator.xor), 0, left, right)

class LeftShift(BinaryMath):
	def __init__(self, f, left, right, bit_width):
		BinaryMath.__init__(self, f, "<<", LeftShiftOp(bit_width), 0, left, right)
		self.bit_width = bit_width

	def extra_create_args(self):
		return [self.bit_width]

class RightShift(BinaryMath):
	def __init__(self, f, left, right, bit_width):
		BinaryMath.__init__(self, f, ">>", RightShiftOp(bit_width), 0, left, right)
		self.bit_width = bit_width

	def extra_create_args(self):
		return [self.bit_width]

class BitOr(BinaryMath):
	def __init__(self, f, left, right):
		BinaryMath.__init__(self, f, "|", PyOp(operator.or_), 0, left, right)

class BitAnd(BinaryMath):
	def __init__(self, f, left, right):
		BinaryMath.__init__(self, f, "&", PyOp(operator.and_), None, left, right)

class LogicalAnd(BinaryMath):
	def __init__(self, f, left, right):
		BinaryMath.__init__(self, f, "&&", LogicalAndOp(), None, left, right)

class CmpLT(BinaryOp):
	def __init__(self, factory, left, right):
		BinaryOp.__init__(self, factory, "<", left, right)

	def evaluate(self, collapser):
		return collapser.lookup(self.left) < collapser.lookup(self.right)

class CmpLEQ(BinaryOp):
	def __init__(self, factory, left, right):
		BinaryOp.__init__(self, factory, "<=", left, right)

	def evaluate(self, collapser):
		return collapser.lookup(self.left) <= collapser.lookup(self.right)

class CmpEQ(BinaryOp):
	def __init__(self, factory, left, right):
		BinaryOp.__init__(self, factory, "==", left, right)

	def evaluate(self, collapser):
		return collapser.lookup(self.left) == collapser.lookup(self.right)

class UnaryOp(Op):
	def __init__(self, factory, op, expr):
		DFGExpr.__init__(self, factory)
		self.op = op
		self.expr = expr

	def __repr__(self):
		return "(%s %s)" % (self.op, self.expr)

	def equal(self, other):
		i = cmp(id(self), id(other))
		if (i==0): return i
		i = cmp(self.__class__, other.__class__)
		if (i!=0): return i
		i = cmp(self.op, other.op)
		if (i!=0): return i
		i = cmp(self.expr, other.expr)
		return i

	def extra_create_args(self):
		return []

	def collapse_dependencies(self):
		return [self.expr]

	def collapse_constants(self, collapser):
		try:
			return self.factory.create(Constant, collapser.evaluate_as_constant(self))
		except NonconstantExpression:
			return self.factory.create(
				self.__class__,
				collapser.lookup(self.expr),
				*self.extra_create_args())

class LogicalNot(UnaryOp):
	def __init__(self, f, expr):
		UnaryOp.__init__(self, f, "Not", expr)

	def evaluate(self, collapser):
		return not collapser.lookup(self.expr)

class BitNot(UnaryOp):
	def __init__(self, f, expr, bit_width):
		UnaryOp.__init__(self, f, "BitNot", expr)
		self.bit_width = bit_width

	def extra_create_args(self):
		return [self.bit_width]

	def evaluate(self, collapser):
		return (~collapser.lookup(self.expr)) & self.bit_width.get_neg1()

class Negate(UnaryOp):
	def __init__(self, f, expr):
		UnaryOp.__init__(self, f, "Negate", expr)

	def __repr__(self):
		return "(- %s)" % self.expr

	def evaluate(self, collapser):
		return 0 - collapser.lookup(self.expr)

class Conditional(Op):
	def __init__(self, factory, cond, valtrue, valfalse):
		DFGExpr.__init__(self, factory)
		self.cond = cond
		self.valtrue = valtrue
		self.valfalse = valfalse

	def __repr__(self):
		return "(? %s %s %s)" % (self.cond, self.valtrue, self.valfalse)

	def equal(self, other):
		i = cmp(id(self), id(other))
		if (i==0): return i
		i = cmp(self.__class__, other.__class__)
		if (i!=0): return i
		i = cmp(self.cond, other.cond)
		if (i!=0): return i
		i = cmp(self.valtrue, other.valtrue)
		if (i!=0): return i
		i = cmp(self.valfalse, other.valfalse)
		return i

	def evaluate(self, collapser):
		cond = collapser.lookup(self.cond)
		if (cond):
			result = collapser.lookup(self.valtrue)
		else:
			result = collapser.lookup(self.valfalse)
		#print "cond %s valfalse %s cond %s result %s" % (self.cond, self.valfalse.evaluate(inputs), cond, result)
		return result

	def collapse_dependencies(self):
		return [self.cond, self.valtrue, self.valfalse]

	def collapse_constants(self, collapser):
		try:
			return self.factory.create(Constant, collapser.evaluate_as_constant(self))
		except:
			return self.factory.create(Conditional,
				collapser.lookup(self.cond),
				collapser.lookup(self.valtrue),
				collapser.lookup(self.valfalse))

class ArrayOp(Op):
	def __init__(self, factory):
		DFGExpr.__init__(self, factory)

	def evaluate(self, collapser):
		# ArrayOps shouldn't appear in expressions; they should be
		# eagerly evaluated away when constructing exprs.
		assert(False)	# unimpl
	
class StorageRef(ArrayOp):
	def __init__(self, factory, type, storage, idx):
		ArrayOp.__init__(self, factory)
		self.type = type
		self.storage = storage
		self.idx = idx

	def __repr__(self):
		return "[%s @ %s :%d]" % (self.type, self.storage, self.idx)

	def is_ptr(self):
		return isinstance(self.type, PtrType)

	def offset_key(self, off):
		assert(not self.is_ptr())
		return StorageKey(self.storage, self.idx+off)

	def key(self):
		if (self.is_ptr()):
			# don't try to write right in if I'm a ptr.
			raise Exception("Trying to eagerly look up ptr %s" % self)
		return StorageKey(self.storage, self.idx)

	def ref(self):
		assert(not self.is_ptr())
			# Can't have a pointer to a pointer, because we can't
			# store pointers.
		return self.factory.create(StorageRef, PtrType(self.type), self.storage, self.idx)

	def deref(self):
		assert(isinstance(self.type, PtrType))
		return self.factory.create(StorageRef, self.type.type, self.storage, self.idx)
