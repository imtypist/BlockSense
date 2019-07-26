#!/usr/bin/python

import pickle
import json
import argparse
import sys
import types
import subprocess
import os
import cProfile
import traceback
#from ArithBackend import ArithBackend
from ArithFactory import ArithFactory
from BooleanFactory import BooleanFactory
from Timing import Timing

mypath = os.path.dirname(__file__)
sys.path.append("%s/../external-code/ply-3.4" % mypath)
sys.path.append("%s/../external-code/pycparser-2.08" % mypath)
import pycparser
#from pycparser.c_ast import *
from pycparser import c_ast
import StringIO

from Symtab import *
from DFG import *
from Struct import *
import BitWidth
from Collapser import Collapser

sys.setrecursionlimit(10000)

class Void(): pass

def ast_show(ast, oneline=False):
	sio = StringIO.StringIO()
	ast.show(buf = sio)
	v = sio.getvalue()
	if (oneline):
		v = v.replace('\n',' ')
	return v

class StaticallyInfiniteLoop(Exception):
	def __init__(self, sanity_limit): self.sanity_limit = sanity_limit
	def __repr__(self): "Loop exceeded %s iters; insane?" % self.sanity_limit
	def __str__(self): return repr(self)
	
class VoidFuncUsedAsExpression(Exception): pass

class ConstantArrayIndexOutOfRange(Exception):
	def __init__(self, s): self.s = s
	def __str__(self): return self.s

class ParameterListMismatchesDeclaration(Exception):
	def __init__(self, s): self.s = s
	def __str__(self): return self.s

class EarlyReturn(Exception):
	def __str__(self): return "Function returns before last statement."

class FuncNotDefined(Exception):
	def __init__(self, s): self.s = s
	def __str__(self): return self.s

class FuncMultiplyDefined(Exception):
	def __init__(self, s): self.s = s
	def __str__(self): return self.s
	
class MalformedOutsourceParameters(Exception): pass

class NoInputSpecDefined(Exception): pass


class State:
	def __init__(self, expr, symtab):
		self.expr = expr
		self.symtab = symtab

	def __repr__(self):
		return "State[%s,%s]" % (self.expr, self.symtab)

class TypeState:
	def __init__(self, type, symtab):
		self.type = type
		self.symtab = symtab

#class ProfileNugget:
#	def __init__(self, collapser, out_expr):
#		self.collapser = collapser
#		self.out_expr = out_expr
#
#	def run(self):
#		self.result = self.collapser.collapse_tree(self.out_expr)

class Vercomp:
	def __init__(self, filename, args, timing):
		self.cpp_arg = args.cpp_arg
		self.timing = timing
		self.loop_sanity_limit = int(args.loop_sanity_limit)
		ignore_overflow = (args.ignore_overflow=="True")
		print "ignore_overflow=%s" % ignore_overflow
		self.bit_width = BitWidth.BitWidth(int(args.bit_width), ignore_overflow)
		self.progress = args.progress

		# memoizer for collapsing exprs to scalar constants
		self.expr_evaluator = ExprEvaluator()

		self.verbose = False
		self.loops = 0

		self.factory = DFGFactory()

#		x = self.dfg(Constant, 7)
#		y = self.dfg(Constant, 7)
#		z = self.dfg(Constant, 8)
#		p = self.dfg(Add, x, y)
#		q = self.dfg(Add, x, y)
#		print (x, y, z, p, q)
#		print map(id, (x, y, z, p, q))

		tmpname = self.gcc_preprocess(filename, self.cpp_arg)
		file = open(tmpname)
		lines = file.read().split("\n")
		lines = self.preprocess(lines)
		cfile = "\n".join(lines)

#		ofp = open("/tmp/foo", "w")
#		ofp.write(cfile)
#		ofp.close()

		parser = pycparser.CParser()
		ast = parser.parse(cfile)
		self.root_ast = ast

		self.output = self.create_expression()

	def gcc_preprocess(self, filename, cpp_arg):
		# returns filename of preprocessed file
		dir = os.path.dirname(filename)
		if (dir==""):
			dir = "."
		tmpname = dir+"/build/"+os.path.basename(filename)+".p"
		def under_to_dash(s):
			if (s[0]=="_"):
				s = "-"+s[1:]
			return s
		if (cpp_arg!=None):
			cpp_args = map(under_to_dash, cpp_arg)
		else:
			cpp_args = []
		# I used to call gcc-4; I'm not sure what machine had two versions
		# or what the issue was, but I leave this comment here as a possible
		# solution to the next sucker who runs into that problem.
		cmd_list = ["gcc"]+cpp_args+["-E", filename, "-o", tmpname]
		print "cmd_list %s" % (" ".join(cmd_list))
		sp = subprocess.Popen(cmd_list)
		sp.wait()
		return tmpname

	def preprocess_line(self, line):
		if (line=='' or line[0]=='#'):
			return ''
		return line.split("//")[0]

	def preprocess(self, lines):
		return map(self.preprocess_line, lines)

	def dfg(self, *args):
		return self.factory.create(*args)

	def decode_scalar_type(self, names):
		if (names==["int"]):
			return IntType()
		elif (names==["unsigned", "int"]):
			return UnsignedType()
		else:
			assert(False)

	def decode_type(self, node, symtab, skip_type_decls=False):
		# this can also declare a new type; returns TypeState
		if (isinstance(node, c_ast.IdentifierType)):
			result = TypeState(self.decode_scalar_type(node.names), symtab)
		elif (isinstance(node, c_ast.Struct)):
			fields = []
			if (node.decls!=None):
				# A struct is being declared here.
				for field_decl in node.decls:
					type_state = self.decode_type(field_decl.type, symtab, skip_type_decls=True)
					symtab = type_state.symtab
					fields.append(Field(field_decl.name, type_state.type))
				struct_type = StructType(node.name, fields)
				symtab.declare(Symbol(node.name), struct_type)
			else:
				# look up the struct
				struct_type = symtab.lookup(Symbol(node.name))
			result = TypeState(struct_type, symtab)
		elif (isinstance(node, c_ast.ArrayDecl)):
			dim_state = self.decode_expression_val(node.dim, symtab)
			symtab = dim_state.symtab
			dimension = self.evaluate(dim_state.expr)
			type_state = self.decode_type(node.type, symtab, skip_type_decls=True)
			symtab = type_state.symtab
			array_type = ArrayType(type_state.type, dimension)
			result = TypeState(array_type, symtab)
		elif (isinstance(node, c_ast.PtrDecl)):
			type_state = self.decode_type(node.type, symtab, skip_type_decls=True)
			symtab = type_state.symtab
			result = TypeState(PtrType(type_state.type), symtab)
		elif (isinstance(node, c_ast.TypeDecl)):
			if (self.verbose):
				print node.__class__
				print node.__dict__
				print ast_show(node)
			assert(skip_type_decls)
			result = self.decode_type(node.type, symtab)
		else:
			print node.__class__
			print node.__dict__
			print ast_show(node)
			assert(False)
		assert(result.type!=None)
		return result

	def create_storage(self, name, store_type, initial_values, symtab):
		# returns State
		storage = Storage(name, store_type.sizeof())
		if (initial_values!=None):
			#print "iv %s st %s" % (len(initial_value), store_type.sizeof())
			if (not (len(initial_values)==store_type.sizeof())):
				print "iv %s st %s" % (len(initial_values), store_type.sizeof())
				assert(False)
			for idx in range(store_type.sizeof()):
				iv = initial_values[idx]
				symtab.declare(StorageKey(storage, idx), iv)
		else:
			for idx in range(store_type.sizeof()):
				symtab.declare(StorageKey(storage, idx), self.dfg(Constant, 0))
		return State(self.dfg(StorageRef, store_type, storage, 0), symtab)

	def declare_variable(self, decl, symtab, initial_value=None):
		# returns Symtab

		if (isinstance(decl.type, c_ast.TypeDecl)):
			decl_type = decl.type.type
		else:
			decl_type = decl.type
		# declare the type
		type_state = self.decode_type(decl_type, symtab)
		symtab = type_state.symtab

		if (decl.name!=None):
			# a variable is being declared;

			store_type = type_state.type

			if (isinstance(store_type, ArrayType)):
				# int w[10] acts like int *w when you access it.
				var_type = PtrType(store_type.type)
			else:
				var_type = store_type

			# decode an initializer
			initial_values = None

			if (decl.init!=None):
				assert(initial_value==None)	# Two sources of initialization?
				if (isinstance(store_type, ArrayType)):
					initial_values = []
					for expr in decl.init.exprs:
						state = self.decode_expression_val(expr, symtab)
						symtab = state.symtab
						initial_values.append(state.expr)
				elif (isinstance(store_type, IntType)):
					initial_values = []
					state = self.decode_expression_val(decl.init, symtab)
					symtab = state.symtab
					initial_values.append(state.expr)
				elif (isinstance(store_type, PtrType)):
					state = self.decode_ref(decl.init, symtab)
					symtab = state.symtab
					initial_value = state.expr
				else:
					print
					print "type %s" % store_type
					print ast_show(decl)
					print decl.init.__class__
					print decl.init.__dict__
					assert(False)
			elif (initial_value):
				if (isinstance(initial_value, StorageRef)
					and not isinstance(store_type, PtrType)):
					def offset(k,i):
						return StorageKey(k.storage, k.idx+i)
					initial_values = map(
						lambda i: symtab.lookup(offset(initial_value.key(), i)),
						range(store_type.sizeof()))
				else:
					initial_values = [initial_value]

			# allocate storage for the new object
			if (not isinstance(store_type, PtrType)):
				#print "initial_values for %s is %s" % (decl.name, initial_values)
				state = self.create_storage(
					decl.name, store_type, initial_values, symtab)
				symbol_value = self.dfg(StorageRef, var_type, state.expr.storage, state.expr.idx)
				symtab = state.symtab
			else:
				if (initial_value != None):
					symbol_value = initial_value
				else:
					symbol_value = self.dfg(StorageRef, store_type, Null(), 0);

			# point the name at it, with appropriate referenciness.
			sym = Symbol(decl.name)
			if (symtab.is_declared(sym)):
				# A duplicate declaration should match in type.
				# But that doesn't exactly work, because there may be a
				# pointer involved.
				if (self.verbose):
					print "Second declaration of %s" % sym
				value = symtab.lookup(sym)
				if (value.type != symbol_value.type):
					print ast_show(decl)
					print "sym %s value type %s symbol_value type %s" % (
						sym, value.type, symbol_value.type)
					assert(False)
				# rewrite the assignment to point at a new storage
				symtab.assign(sym, symbol_value)
			else:
				symtab.declare(sym, symbol_value)

		return symtab

	def create_global_symtab(self):
		symtab = Symtab()
		for (iname,obj) in self.root_ast.children():
			if (isinstance(obj, c_ast.Decl)):
#				print
#				print obj.__dict__
#				print ast_show(obj)
				if (isinstance(obj.type, c_ast.FuncDecl)):
					if (self.verbose):
						print "Ignoring funcdecl %s for now" % (obj.name)
					pass
				else:
					if (self.verbose):
						print
						print ast_show(obj)
					symtab = self.declare_variable(obj, symtab)
			elif (isinstance(obj, c_ast.FuncDef)):
				if (self.verbose):
					print "Ignoring funcdef %s for now" % (obj.decl.name)
				pass
			else:
				print obj.__class__
				assert(False)
		symtab.declare(PseudoSymbol("_unroll"), self.dfg(Undefined))
		return symtab

	def declare_scalar(self, name, value, symtab):
		print "decl scalar: %s" % (name,)
		storage = Storage(name, 1)
		symtab.declare(StorageKey(storage, 0), value)
		symtab.declare(Symbol(name), ArrayVal(storage, 0))

	def create_scope(self, param_decls, param_exprs, symtab):
		scope_symtab = Symtab(symtab, scope=set())
		if (not (len(param_decls)==len(param_exprs))):
			raise ParameterListMismatchesDeclaration(
				"declared with %d parameters; called with %d parameters" %
				(len(param_decls), len(param_exprs)))
		for parami in range(len(param_decls)):
			param_decl = param_decls[parami]
			#print "create_scope declares %s as %s" % (ast_show(param_decl, oneline=True), param_exprs[parami])
			scope_symtab = self.declare_variable(
				param_decl, scope_symtab, initial_value=param_exprs[parami])
		if (self.verbose):
			print("scope symtab is: %s" % scope_symtab)
		return scope_symtab

	def decode_ptr_key(self, node):
		# when assigning to a pointer, you must be assigning to a raw
		# symbol; runtime Storage() can't hold pointers in our language.
		# returns Key
		if (isinstance(node, c_ast.ID)):
			return Symbol(node.name)
		else:
			assert(False)

	def decode_ref(self, node, symtab):
		# returns State
		if (isinstance(node, c_ast.ID)):
			result = State(symtab.lookup(Symbol(node.name)), symtab)
		elif (isinstance(node, c_ast.StructRef)):
			result = self.decode_struct_ref(node, symtab)
		elif (isinstance(node, c_ast.ArrayRef)):
			result = self.decode_array_ref(node, symtab)
		elif (isinstance(node, c_ast.UnaryOp)):
			result = self.decode_expression(node, symtab)
		else:
			print
			print ast_show(node)
			print node.__class__
			print node.__dict__
			assert(False)
		if (self.verbose):
			print "decode_ref %s %s giving %s" % (node.__class__, ast_show(node), result.expr)
		return result

	def decode_struct_ref(self, structref, symtab):
		# returns State
		sref_state = self.decode_ref(structref.name, symtab)
		storageref = sref_state.expr
		symtab = sref_state.symtab
		if (structref.type=="->"):
			prior_storageref = storageref
			storageref = prior_storageref.deref()
			if (self.verbose):
				print "decode_struct_ref %s turns %s into %s" % (
					ast_show(structref), prior_storageref, storageref)
		elif (structref.type=="."):
			pass
		else:
			assert(False)
		struct = storageref.type
		assert(isinstance(struct, StructType))
		assert(isinstance(structref.field, c_ast.ID))
		field = struct.get_field(structref.field.name)
		fieldstorage = self.dfg(StorageRef,
			field.type,
			storageref.storage,
			storageref.idx + struct.offsetof(field.name));
		return State(fieldstorage, symtab)

	def decode_array_ref(self, arrayref, symtab):
		# returns State(StorageRef, Symtab)
		subscript_state = self.decode_expression_val(arrayref.subscript, symtab)
		symtab = subscript_state.symtab
		try:
			subscript_val = self.evaluate(subscript_state.expr)
		except NonconstantExpression, ex:
			msg = "Array subscript isn't a constant expression at %s;\nexpr is %s" % (
				arrayref.subscript.coord, subscript_state.expr)
			raise NonconstantArrayAccess(msg)
		#print "subscript_val expr %s == %s" % (subscript_state.expr, subscript_val)
		state = self.decode_ref(arrayref.name, symtab)
		if (self.verbose):
			print "array_ref got %s" % state.expr
		name_storageref = state.expr
		if (not isinstance(name_storageref, StorageRef)):
			print ast_show(arrayref)
			print "name: %s" % arrayref.name.name
			print "storageref: %s" % name_storageref
			assert(False)
		symtab = state.symtab
		if (self.verbose):
			print ast_show(arrayref)
			print "name_storageref: %s %s" % (name_storageref.type, name_storageref.type.__class__)
		if (isinstance(name_storageref.type, ArrayType)):
			element_type = name_storageref.type.type
		elif (isinstance(name_storageref.type, PtrType)):
			element_type = name_storageref.type.type
		else:
			print "name_storageref is %s" % name_storageref
			assert(False)
		array_storageref = self.dfg(StorageRef,
			element_type,
			name_storageref.storage,
			name_storageref.idx + subscript_val*element_type.sizeof())
		if (self.verbose):
			print "arrayref --> %s" % array_storageref
		return State(array_storageref, symtab)

	def eager_lookup(self, key, symtab):
		val = symtab.lookup(key)
		if (isinstance(val, StorageRef)):
			newval = symtab.lookup(val.key())
			#print "eager_lookup converts %s to %s" % (val, newval)
			val = newval
		#print "eager_lookup returns %s" % val
		return val

	def coerce_value(self, expr, symtab):
		# TODO is this correct? The expr may be a ref storage acquired early
		# in the eval process, then the symtab changed to update
		# that location. Hmm.
		# This is getting super-brittle. I really have no idea
		# which symtab the expr should be evaluated against to coerce
		# it down to a value. Probably should have bundled that into a
		# wrapper object. Ugh.
		if (isinstance(expr, StorageRef)):
			key = expr.key()
			expr = symtab.lookup(key)
		return expr
		
	# look up an expression, but evaluate away any StorageRef,
	# so that the resulting expr can be incorporated as arguments into
	# other exprs.
	def decode_expression_val(self, expr, symtab, void=False):
		state = self.decode_expression(expr, symtab, void=void)
		state = State(self.coerce_value(state.expr, symtab), state.symtab)
		#print "decode_expression_val returns %s" % state.expr
		return state

	def decode_expression(self, expr, symtab, void=False):
		# returns State
		if (isinstance(expr, c_ast.UnaryOp)):
			if (expr.op=="*"):
				state = self.decode_ref(expr.expr, symtab)
				return State(state.expr.deref(), state.symtab)
			else:
				if (expr.op=="-"):
					state = self.decode_expression_val(expr.expr, symtab)
					return State(self.dfg(Negate, state.expr), state.symtab)
				elif (expr.op=="~"):
					state = self.decode_expression_val(expr.expr, symtab)
					return State(self.dfg(BitNot, state.expr, self.bit_width), state.symtab)
				elif (expr.op=="!"):
					state = self.decode_expression_val(expr.expr, symtab)
					return State(self.dfg(LogicalNot, state.expr), state.symtab)
				elif (expr.op=="&"):
					sub_state = self.decode_ref(expr.expr, symtab)
					symtab = sub_state.symtab
					if (self.verbose):
						print "for %s got expr %s %s" % (expr.expr, sub_state.expr, type(sub_state.expr))
					ref = sub_state.expr.ref()
					if (self.verbose):
						print "&-op decodes to %s" % ref
					return State(ref, symtab)
			print "expr.op == %s" % expr.op
			assert(False)
		elif (isinstance(expr, c_ast.BinaryOp)):
			left_state = self.decode_expression_val(expr.left, symtab)
			right_state = self.decode_expression_val(expr.right, left_state.symtab)
			#print "right_state  is %s" % right_state.expr
			if (expr.op=="+"):
				expr = self.dfg(Add, left_state.expr, right_state.expr)
			elif (expr.op=="-"):
				# Hmm. Have to start thinking about representations vs. overflows...
				expr = self.dfg(Subtract, left_state.expr, right_state.expr)
			elif (expr.op=="*"):
				expr = self.dfg(Multiply, left_state.expr, right_state.expr)
			elif (expr.op=="<"):
				expr = self.dfg(CmpLT, left_state.expr, right_state.expr)
			elif (expr.op=="<="):
				expr = self.dfg(CmpLEQ, left_state.expr, right_state.expr)
			elif (expr.op=="=="):
				expr = self.dfg(CmpEQ, left_state.expr, right_state.expr)
			elif (expr.op==">"):
				# NB the argument order is swapped.
				expr = self.dfg(CmpLT, right_state.expr, left_state.expr)
			elif (expr.op==">="):
				# NB the argument order is swapped.
				expr = self.dfg(CmpLEQ, right_state.expr, left_state.expr)
			elif (expr.op=="/"):
				expr = self.dfg(Divide, left_state.expr, right_state.expr)
			elif (expr.op=="%"):
				expr = self.dfg(Modulo, left_state.expr, right_state.expr)
			elif (expr.op=="^"):
				expr = self.dfg(Xor, left_state.expr, right_state.expr)
			elif (expr.op=="<<"):
				expr = self.dfg(LeftShift, left_state.expr, right_state.expr, self.bit_width)
			elif (expr.op==">>"):
				expr = self.dfg(RightShift, left_state.expr, right_state.expr, self.bit_width)
			elif (expr.op=="|"):
				expr = self.dfg(BitOr, left_state.expr, right_state.expr)
			elif (expr.op=="&"):
				expr = self.dfg(BitAnd, left_state.expr, right_state.expr)
			elif (expr.op=="&&"):
				expr = self.dfg(LogicalAnd, left_state.expr, right_state.expr)
			else:
				print
				print ast_show(expr)
				print expr.__class__
				print expr.__dict__
				assert(False)	# unimpl
			return State(expr, right_state.symtab)
		elif (isinstance(expr, c_ast.Constant)):
			assert(expr.type=="int")
			if (expr.value.startswith("0x")):
				literal = int(expr.value, 16)
				#print "parsed %s as 0x %x" % (expr.value, literal)
			else:
				literal = int(expr.value, 10)
			return State(self.dfg(Constant, literal), symtab)
		elif (isinstance(expr, c_ast.ArrayRef)
			or isinstance(expr, c_ast.StructRef)):
			ref_val_state = self.decode_ref(expr, symtab)
			ref_val = ref_val_state.expr
			assert(isinstance(ref_val, StorageRef))
			# And now it's okay to return StorageRefs; the caller
			# must coerce to a value if that's what he needs.
			return State(ref_val, ref_val_state.symtab)
		elif (isinstance(expr, c_ast.ID)):
			return State(symtab.lookup(Symbol(expr.name)), symtab)
		elif (isinstance(expr, c_ast.FuncCall)):
			state = self.decode_funccall(expr, symtab)
			if (not void and isinstance(state.expr, Void)):
				print ast_show(expr)
				raise VoidFuncUsedAsExpression()
			return state
		else:
			pass
		print Constant
		print expr.__class__
		print expr.__dict__
		assert(False)

	def decode_funccall(self, expr, symtab):
		func_arg_exprs = []
		prev_symtab = symtab
		try:
			exprs = expr.args.exprs
		except:
			exprs = []
		for arg_expr in exprs:
			state = self.decode_expression(arg_expr, prev_symtab)
			# NB we can allow exprs here that are lvalues (eg ptr StorageRefs),
			# as we'll be writing them back into a symtab like an assignment.
			#print "state %s type %s" % (state, type(state))
			if (self.verbose):
				print "arg %s expands to %s" % (ast_show(arg_expr), state.expr)
			func_arg_exprs.append(state.expr)
			prev_symtab = state.symtab
		func_def = self.find_func(expr.name.name)
		try:
			param_decls = func_def.decl.type.args.params
		except:
			param_decls = []

		call_symtab = self.create_scope(
			param_decls, func_arg_exprs, prev_symtab)
		side_effect_symtab = self.transform_compound(
			func_def.body, call_symtab, func_scope=True)
		result_symtab = call_symtab.apply_changes(side_effect_symtab, prev_symtab)
		#print "side_effect_symtab: %s"%side_effect_symtab
		try:
			result_expr = side_effect_symtab.lookup(PseudoSymbol("return"))
		except UndefinedSymbol:
			result_expr = Void()
		return State(result_expr, result_symtab)

	def transform_assignment(self, stmt, symtab):
		# returns symtab
		if (stmt.op=="="):
			right_state = self.decode_expression(stmt.rvalue, symtab)
			symtab = right_state.symtab
			expr = right_state.expr
			#print "ta: %s decodes to %s" % (ast_show(stmt.rvalue, 1), expr)
		elif (len(stmt.op)==2 and stmt.op[1]=='='):
			# These tricksy assignments won't work for pointer values just yet.
			left_state = self.decode_expression_val(stmt.lvalue, symtab)
			right_state = self.decode_expression_val(stmt.rvalue, left_state.symtab)
			if (stmt.op=="+="):
				if (self.verbose):
					print "stmt.rvalue %s" % ast_show(stmt.rvalue)
					print "left_state %s right_state %s" % (left_state, right_state)
				expr = self.dfg(Add, left_state.expr, right_state.expr)
			elif (stmt.op=="-="):
				expr = self.dfg(Subtract, left_state.expr, right_state.expr)
			elif (stmt.op=="*="):
				expr = self.dfg(Multiply, left_state.expr, right_state.expr)
			elif (stmt.op=="|="):
				expr = self.dfg(BitOr, left_state.expr, right_state.expr)
			else:
				assert(False)
			symtab = right_state.symtab
		else:
			assert(False)

		#print "transform_assignment symtab is %s" % symtab
		lvalue = stmt.lvalue
		if (isinstance(lvalue, c_ast.ID) and lvalue.name == "_unroll"):
			sym = PseudoSymbol(lvalue.name)
			symtab.assign(sym, expr)
		else:
			lvalue_state = self.decode_ref(lvalue, symtab)
			symtab = lvalue_state.symtab
			#print "node is %s expr is %s" % (ast_show(lvalue), lvalue_state.expr)
			#print "type is %s" % lvalue_state.expr.type
			#print "transform_assignment lvalue %s type %s" % (lvalue_state.expr, lvalue_state.expr.type)
			if (lvalue_state.expr.is_ptr()):
				#print "lvalue was a ptr, it now points elsewhere"
				sym = self.decode_ptr_key(lvalue)
				symtab.assign(sym, expr)
			else:
				sym = lvalue_state.expr.key()
				sizeof = lvalue_state.expr.type.sizeof()
				if (sizeof > 1):
					#print "Transferring %d values to %s" % (sizeof, lvalue_state.expr)
					lkey = lvalue_state.expr
					rkey = expr
					for i in range(lvalue_state.expr.type.sizeof()):
						symtab.assign(lkey.offset_key(i),
							symtab.lookup(rkey.offset_key(i)))
				else:
					# int-valued expression
					#print "sym is %s (%s)" % (sym, sym.__class__)
					expr = self.coerce_value(expr, symtab)
					symtab.assign(sym, expr)
			#print "transform_assignment %s" % ast_show(stmt, oneline=1)
			#print "+transform_assignment sym %s val %s" % (sym, expr)
		return symtab

	def transform_if(self, statement, symtab):
		# returns symtab
		cond_state = self.decode_expression_val(statement.cond, symtab)
		symtab = cond_state.symtab

		try:
			# If condition is statically evaluable. Just run one branch.
			cond_val = self.evaluate(cond_state.expr)
			if (cond_val):
				return self.transform_statement(statement.iftrue, symtab)
			elif (statement.iffalse!=None):
				return self.transform_statement(statement.iffalse, symtab)
			else:
				# no-op.
				return symtab
		except NonconstantExpression, ex:
			# If condition is dynamic. Run both branches (in dedicated scopes)
			# and make resulting storage updates conditional on the
			# dynamic condition.

			then_scope = Symtab(symtab, scope=set())
			then_symtab = self.transform_statement(
				statement.iftrue, then_scope)
			else_scope = Symtab(symtab, scope=set())
			if (statement.iffalse!=None):
				else_symtab = self.transform_statement(
					statement.iffalse, else_scope)
			else:
				else_symtab = else_scope
			#new_symtab = Symtab(symtab)	# DEAD
			new_symtab = symtab
			modified_idents = then_scope.scope.union(else_scope.scope)
			#print "If modifies %s" % modified_idents
			for key in modified_idents:
				expr = self.dfg(Conditional, cond_state.expr, then_symtab.lookup(key), else_symtab.lookup(key))
				new_symtab.assign(key, expr)
			return new_symtab

	def transform_while(self, while_stmt, symtab):
		return self.transform_loop(while_stmt.cond, while_stmt.stmt, symtab)

	def transform_for(self, for_stmt, symtab):
		working_symtab = self.transform_statement(for_stmt.init, symtab)
		return self.transform_loop(
			for_stmt.cond,
			c_ast.Compound([for_stmt.stmt, for_stmt.next]),
			working_symtab)

	def loop_msg(self, m):
		if ((self.progress or self.verbose) and self.loops<2):
			print "%s%s" % ("  "*self.loops, m)

	def unroll_static_loop(self, cond, body_compound, symtab):
		self.loops+=1
		sanity = -1
		working_symtab = symtab
		#self.loop_msg("___start___")
		while (True):
			sanity += 1
			if (sanity > self.loop_sanity_limit):
				print cond_val
				print cond_state.expr
				print ast_show(cond)
				raise StaticallyInfiniteLoop(self.loop_sanity_limit)
			cond_state = self.decode_expression_val(cond, working_symtab)
			# once a condition is statically evaluable, we assume it
			# always is.
			cond_val = self.evaluate(cond_state.expr)
			if (not cond_val):
				break
			if (self.verbose):
				print "loop body is:"
				print ast_show(body_compound)
			if (sanity>0 and (sanity & 0x3f)==0):
				self.loop_msg("static iter %d" % sanity)
			working_symtab = self.transform_statement(
				body_compound, cond_state.symtab)
		#self.loop_msg("___end___")
		self.loops-=1
		return working_symtab

	def unroll_dynamic_loop(self, cond, body_compound, symtab):
		self.loops+=1
		try:
			_unroll_val = self.evaluate(symtab.lookup(PseudoSymbol("_unroll")))
		except UndefinedExpression:
			print "At line %s:" % cond.coord.line
			raise
		# build up nested conditional scopes
		working_symtab = symtab
		cond_stack = []
		scope_stack = []
		for i in range(_unroll_val):
			self.loop_msg("dyn iter %s" % i)
			cond_state = self.decode_expression_val(cond, working_symtab)
			scope = Symtab(cond_state.symtab, scope=set())
			cond_stack.append(cond_state.expr)
			scope_stack.append(scope)
			#print "rep %d; k is: %s" % (i, self.eager_lookup(Symbol("k"), scope))
			working_symtab = self.transform_statement(body_compound, scope)

		assert(_unroll_val == len(cond_stack))

		while (len(cond_stack)>0):
			cond = cond_stack.pop()
			scope = scope_stack.pop()
			modified_idents = scope.scope
			#applied_symtab = Symtab(working_symtab)	#DEAD
			applied_symtab = working_symtab
			for ref in modified_idents:
				if (self.verbose):
					print
					print "cond: %s" % cond
					print "iftrue: %s" % working_symtab.lookup(ref)
					print "iffalse: %s" % scope.parent.lookup(ref)
				applied_symtab.assign(ref,
					self.dfg(Conditional, cond, working_symtab.lookup(ref), scope.parent.lookup(ref)))
			working_symtab = applied_symtab
		self.loops-=1
		return working_symtab

	def transform_loop(self, cond, body_compound, symtab):
		working_symtab = symtab

		cond_state = self.decode_expression_val(cond, working_symtab)
		try:
			try:
				cond_val = self.evaluate(cond_state.expr)
			except NonconstantExpression:
				pass
			except Exception, ex:
				print "expr is %s" % cond_state.expr
				raise
			try:
				return self.unroll_static_loop(cond, body_compound, symtab)
			except NonconstantExpression, unexpected_nce:
				traceback.print_exc(unexpected_nce)
				print "\n---------\n"
				raise Exception("Unexpected NonconstantExpression; it leaked up from some subexpression evaluation?")
		except NonconstantExpression, ex:
			#print "Condition is dynamic (ex %s):" % repr(ex)
			#print ":: ",ast_show(cond)
			#print ":: ",cond_state.expr

			# condition can't be evaluated statically at top of loop.
			# [NB In principle, it might become statically-evaluable later
			# (eg while (x<5 and x<size)), but it's not clear we really
			# want to encourage programming that way just to get the
			# automatic unroll limit, since it would involve emitting
			# a runtime evaluation for x<5.]
			# So, we assume that the only way to limit the loop is to
			# use the _unroll hint.
			return self.unroll_dynamic_loop(cond, body_compound, symtab)

	def transform_compound(self, body, outer_symtab, func_scope=False):
		# func_scope says that it's okay to return a value
		# from (the end of) this compound block.
		# returns Symtab
		working_symtab = outer_symtab
		#print "compound %s is %s" % (body.__class__, ast_show(body))
		assert(isinstance(body, c_ast.Compound))
		children = body.children()
		for stmt_idx in range(len(children)):
			(name, statement) = children[stmt_idx]
			return_allowed = (func_scope and stmt_idx == len(children)-1)
			working_symtab = self.transform_statement(statement, working_symtab, return_allowed = return_allowed)
		return working_symtab

	def transform_statement(self, statement, working_symtab, return_allowed=False):
		if (isinstance(statement, c_ast.Assignment)):
			working_symtab = self.transform_assignment(
				statement, working_symtab)
		elif (isinstance(statement, c_ast.If)):
			working_symtab = self.transform_if(statement, working_symtab)
		elif (isinstance(statement, c_ast.Return)):
			if (not return_allowed):
				raise EarlyReturn()
			return_state = self.decode_expression_val(
				statement.expr, working_symtab)
			#working_symtab = Symtab(return_state.symtab)	# DEAD
			working_symtab = return_state.symtab
			working_symtab.declare(PseudoSymbol("return"), return_state.expr)
		elif (isinstance(statement, c_ast.Decl)):
			# NB modify in place. I think that's actually okay,
			# when we're not forking them. Which I guess we never
			# do, since we use them and discard them.
			working_symtab = self.declare_variable(statement, working_symtab)
		elif (isinstance(statement, c_ast.FuncCall)):
			# an expression whose result is discarded; only side-effects
			# matter.
			state = self.decode_expression_val(statement, working_symtab, void=True)
			working_symtab = state.symtab
		elif (isinstance(statement, c_ast.For)):
			working_symtab = self.transform_for(statement, working_symtab)
		elif (isinstance(statement, c_ast.While)):
			working_symtab = self.transform_while(statement, working_symtab)
		elif (isinstance(statement, c_ast.Compound)):
			working_symtab = self.transform_compound(statement, working_symtab)
		else:
			print "class: ",statement.__class__
			print "dict: ",statement.__dict__
			print "ast: ",ast_show(statement)
			assert(False)	# unimpl statement type
		if (self.verbose):
			print "after statement %s, symtab:" % ast_show(statement, oneline=True)
			print "  %s"%working_symtab
		return working_symtab

#DEAD
#	def decode_label_hacks(self, func_name):
#		try:
#			input = self.find_func(func_name)
#		except:
#			raise NoInputSpecDefined();
#		iv = IDVisitor()
#		iv.visit(input)
#		return iv.ids

	def print_expression(self):
		print "Final expr assignments:"
		for (name, value) in self.output:
			print "%s => %s" % (name, value)

	def make_global_storage(self, node, symtab):
		# returns State
		assert(isinstance(node, c_ast.Decl))
		type_state = self.decode_type(node.type, symtab)
		ptr_type = type_state.type
		assert(isinstance(ptr_type, PtrType))
		store_type = ptr_type.type
		state = self.create_storage(node.name, store_type, None, symtab)
		symtab = state.symtab
		symbol_value = self.dfg(StorageRef, ptr_type, state.expr.storage, state.expr.idx)
		return State(symbol_value, symtab)

	def root_funccall(self, symtab):
		# returns a StorageRef
		func_def = self.find_func("outsource")
		param_decls = func_def.decl.type.args.params

		if (len(param_decls)==3):
			has_nizk = True
		elif (len(param_decls)==2):
			has_nizk = False
		else:
			raise MalformedOutsourceParameters()

		INPUT_ARG_IDX = 0
		NIZK_ARG_IDX = 1
		OUTPUT_ARG_IDX = -1	# last, regardless of if there's a nizk input
		arg_exprs = []
		input_state = self.make_global_storage(param_decls[INPUT_ARG_IDX], symtab)
		state = input_state
		arg_exprs.append(input_state.expr)
		if (has_nizk):
			nizk_state = self.make_global_storage(param_decls[NIZK_ARG_IDX], state.symtab)
			state = nizk_state
			arg_exprs.append(nizk_state.expr)
		output_state = self.make_global_storage(param_decls[OUTPUT_ARG_IDX], state.symtab)
		state = output_state
		arg_exprs.append(state.expr)
		symtab = state.symtab

		symtab = self.create_scope(
			param_decls, arg_exprs, symtab)

		def create_input_keys(param_decl_idx, class_):
			in_sym = Symbol(param_decls[param_decl_idx].name)
			#print "sym %s" % in_sym
			input_storage_ref = symtab.lookup(in_sym).deref()
			#print "input_storage_ref %s" % input_storage_ref
			#print "root_funccall symtab: %s" % symtab
			assert(input_storage_ref.idx==0)
			input_list = []
			for idx in range(input_storage_ref.type.sizeof()):
				#print "create_input_keys(%s)[%d]" % (class_.__name__, idx)
				sk = StorageKey(input_storage_ref.storage, idx)
				input = self.dfg(class_, sk)
				input_list.append(input)
				symtab.assign(sk, input)
			return input_list
		
		self.inputs = create_input_keys(INPUT_ARG_IDX, Input)
		self.nizk_inputs = []
		if (has_nizk):
			self.nizk_inputs = create_input_keys(NIZK_ARG_IDX, NIZKInput)

		#print "root_funccall symtab: %s" % symtab
		result_symtab = self.transform_compound(
			func_def.body, symtab, func_scope=True)
		#print "root_funccall result_symtab:"
		#result_symtab.dbg_dump_path()
			
		out_sym = Symbol(param_decls[OUTPUT_ARG_IDX].name)
		output = result_symtab.lookup(out_sym).deref()
		return State(output, result_symtab)

	def create_expression(self):
		global_symtab = self.create_global_symtab()

#		print "Setting up inputs; symtab: %s" % global_symtab
#		# mark all input storage locations as non-constant inputs
#		for name in self.decode_label_hacks("_input"):
#			v = global_symtab.lookup(Symbol(name))
#			assert(isinstance(v, ArrayOp))
#			assert(v.idx == 0)
#			for idx in range(v.storage.size):
#				sk = StorageKey(v.storage, idx)
#				print "assigning %s" % sk
#				global_symtab.assign(sk, Input(sk))
		outsource_func = self.find_func("outsource")
		if (self.verbose):
			print "global_symtab: %s" % global_symtab
		self.timing.phase("root_funccall")
		out_state = self.root_funccall(global_symtab)
		output_storage_ref = out_state.expr

		# memoizer for collapsing exprs to minimal expressions (with
		# no expressions purely of constant terms).
		collapser = ExprCollapser(self.expr_evaluator)

		self.timing.phase("collapse_output")
		if (self.progress):
			print "collapsing output"
		output = []
		for idx in range(output_storage_ref.type.sizeof()):
			#print "working on output %d" % idx
			self.timing.phase("collapse_output %d" % idx)
			sk = StorageKey(output_storage_ref.storage, idx)
			try:
				out_expr = self.eager_lookup(sk, out_state.symtab)
#				global profile_nugget
#				profile_nugget = ProfileNugget(collapser, out_expr)
#				if (idx in []): #[3, 47]):
#					outfn = "/tmp/profiler.%d" % idx
#					cProfile.run('profile_nugget.run()', outfn)
#				else:
#					profile_nugget.run()
#				value = profile_nugget.result
				value = collapser.collapse_tree(out_expr)
			except TypeError:
				#print out_expr
				raise
			output.append((sk, value))
		return output

	def find_func(self, goal_name):
		def is_match(pr):
			(name,node) = pr
			try:
				return node.decl.name==goal_name
			except:
				return False
		matches = filter(is_match, self.root_ast.children())
		if (len(matches)==0):
			raise FuncNotDefined(goal_name)
		elif (len(matches)>1):
			raise FuncMultiplyDefined(goal_name)
		return matches[0][1]

	def evaluate(self, expr):
		return self.expr_evaluator.collapse_tree(expr)

class ExprEvaluator(Collapser):
	def __init__(self):
		Collapser.__init__(self)

	def get_dependencies(self, expr):
		return expr.collapse_dependencies()

	def collapse_impl(self, expr):
		return expr.evaluate(self)

	def get_input(self, key):
		raise NonconstantExpression()

class ExprCollapser(Collapser):
	def __init__(self, evaluator):
		Collapser.__init__(self)
		self.evaluator = evaluator

	def get_dependencies(self, expr):
#		timing = Timing("ExprCollapser.get_dependencies")
		result = expr.collapse_dependencies()
#		timing.phase("done")
		return result

	def collapse_impl(self, expr):
#		timing = Timing("ExprCollapser.collapse_impl")
		result = expr.collapse_constants(self)
#		timing.phase("done")
#		if (timing.prev_elapsed_sec > 0.005):
#			print "Slow expression to collapse was: %s" % expr
		return result

	def evaluate_as_constant(self, expr):
		return self.evaluator.collapse_tree(expr)

def main(argv):
	parser = argparse.ArgumentParser(description='Compile C to QSP/QAP')
	parser.add_argument('cfile', metavar='<cfile>',
		help='a C file to compile')
	parser.add_argument('--print', dest='print_exprs',
		help="print output expressions on stdout")
	parser.add_argument('--il', dest='il_file',
		help='intermediate circuit output file')
	parser.add_argument('--json', dest='json_file',
		help='json version of intermediate circuit output file')
	parser.add_argument('--arith', dest='arith_file',
		help='arithmetic circuit output file')
	parser.add_argument('--bit-width', dest='bit_width',
		help='bit width -- affects bitwise operator semantics and arithmetic circuit output', default=32)
	parser.add_argument('--bool', dest='bool_file',
		help='boolean circuit output file')
	parser.add_argument('--ignore-overflow', dest='ignore_overflow',
		help='ignore field-P overflows; never truncate', default=False)
	parser.add_argument('--cpparg', dest='cpp_arg', nargs="*",
		help='extra arguments to C preprocessor')
	parser.add_argument('--loop-sanity-limit', dest='loop_sanity_limit',
		help='limit on statically-measured loop unrolling', default=1000000)
	parser.add_argument('--progress', dest='progress',
		help='print progress messages during compilation')

	args = parser.parse_args(argv)

	timing = Timing(args.cfile, enabled=False)
	try:
		vercomp = Vercomp(args.cfile, args, timing)
	except Exception,ex:
		print repr(ex)
		raise
		print "DFG total count: %s flatbytes: %s" % (total.count, total.flatbytes)
		flats = total.flats
		flats.sort()
		flats = flats[::-1]
		print("Biggest flat: len %s %s" % (flats[0][0], str(flats[0][1])[0:160]))
		raise

	if (args.print_exprs):
		timing.phase("print_expression")
		vercomp.print_expression()

	if (args.il_file!=None):
		timing.phase("emit_il")
		fp = open(args.il_file, "w")
#		print "\n  ".join(types.__dict__.keys())
#		x = vercomp.output[0][1].left.left
#		print x
#		print x.__class__
#		print x.__dict__
#		pickle.dump(x, fp)
		pickle.dump(vercomp.output, fp)
		fp.close()

	if (args.json_file!=None):
		timing.phase("emit_json")
		fp = open(args.json_file, "w")
		json.dump(vercomp.output, fp)
		fp.close()

	if (args.arith_file!=None):
		timing.phase("emit_arith")
		if (vercomp.progress):
			print "Compilation complete; emitting arith."
		ArithFactory(args.arith_file, vercomp.inputs, vercomp.nizk_inputs, vercomp.output, vercomp.bit_width)

	if (args.bool_file!=None):
		timing.phase("emit_bool")
		if (vercomp.progress):
			print "Compilation complete; emitting bool."
		BooleanFactory(args.bool_file, vercomp.inputs, vercomp.nizk_inputs, vercomp.output, vercomp.bit_width)

	timing.phase("done")

def testcase():
	args = "one-matrix.c --bit-width 32 --cpparg Ibuild/ DPARAM=3 DBIT_WIDTH=32 --ignore-overflow True".split(' ')
	main(args)

if (__name__=="__main__"):
	main(sys.argv[1:])
	#cProfile.run('main()')

