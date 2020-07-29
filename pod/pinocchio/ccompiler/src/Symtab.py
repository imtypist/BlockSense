from Storage import *
import DFG

class UndefinedSymbol(Exception):
	def __init__(self, s): self.s = repr(s)
	def __str__(self): return "UndefinedSymbol(%s)" % self.s
	def __repr__(self): return "UndefinedSymbol(%s)" % self.s

class DuplicateDeclaration(Exception):
	def __init__(self, s): self.s = repr(s)
	def __str__(self): return "DuplicateDeclaration(%s)" % self.s
	def __repr__(self): return "DuplicateDeclaration(%s)" % self.s

##############################################################################

max_depth = 0

class Symtab:
	def __init__(self, parent=None, scope=None):
		self.parent = parent
		if (parent!=None):
			self.depth = self.parent.depth + 1
			#print "depth %s" % self.depth
			global max_depth
			if (self.depth > max_depth):
				#print "symtabs %d deep" % self.depth
				max_depth += 1
				if (max_depth > 100):
					# too-deep symtabs are inefficient, as we walk up
					# the tree doing note_assignment()s.
					raise Exception("why!?")
		else:
			self.depth = 0
		#print "depth %s parent %s" % (self.depth, self.parent)
		self.decl_table = set()
			# set of declared variable names
		self.assign_table = {}
			# maps string identifiers to List(DFGExpr)
			# NB every variable (storage location) is an array.
		self.scope = scope
			# if set, collects assigned keys that are declared above self,
			# for later if-merging.
	
	def is_declared(self, key):
		return key in self.decl_table

	def declare(self, key, value):
		self._kvcheck(key, value)
		if (key in self.decl_table):
			raise DuplicateDeclaration(key)
		self.decl_table.add(key)
		assert(value!=None)
		self.assign_table[key] = value

	def _kvcheck(self, key, value):
		if (isinstance(key, StorageKey)):
			assert(not isinstance(value, DFG.StorageRef))

	def assign(self, key, value):
		assert(isinstance(key, Key))
		self._kvcheck(key, value)
		self._propagate(key)
		self.assign_table[key] = value
		try:
			self.note_assignment(key)
		except:
			print "me: %s max: %s" % (self.depth, max_depth)
			raise
		
	def dbg_dump_path(self):
		p = self
		i = 0
		while (p!=None):
			print "  "*i+repr(p)
			i += 2
			p = p.parent

	def note_assignment(self, key):
		if (key in self.decl_table):
			# declared here
			pass
		else:
			if (self.scope!=None):
				self.scope.add(key)
			if (self.parent!=None):
				self.parent.note_assignment(key)

	def lookup(self, key):
		assert(isinstance(key, Key))
		return self._propagate(key)

	def _fetch(self, key):
		if (key in self.assign_table):
			return self.assign_table[key]
		if (self.parent==None):
			raise UndefinedSymbol(key)
		return self.parent._fetch(key)

	def _propagate(self, key):
		value = self._fetch(key)
		self.assign_table[key] = value
		return value

	def __repr__(self):
		ding = ""
		if (self.scope!=None):
			ding = "(SCOPE)"
		return "%s%s" % (self.assign_table, ding)

	def apply_changes(self, extract_symtab, apply_symtab):
		#result_symtab = Symtab(apply_symtab)
		result_symtab = apply_symtab
		for key in self.scope:
			value = extract_symtab.lookup(key)
			#print "passing value %s to result key %s" % (value, key)
			result_symtab.assign(key, value)
		return result_symtab
