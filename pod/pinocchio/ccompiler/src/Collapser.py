import types
from Timing import Timing

class Collapser:
	def __init__(self):
		self.table = {}
		self.dbg_last_calc = None
		self.dbg_has_last = False

	def get_dependencies(self, key):
		raise Exception("abstract method")

	def collapse_impl(self, key):
		raise Exception("abstract method")

	def collapse_tree(self, key):
		timing = Timing("collapse_tree", enabled=False)
		stack = [key]
		loop_count = 0
		while (len(stack)>0):
			timing.phase("collapser loop # %s setup" % loop_count)
			loop_count += 1
			key = stack[-1]
			if (key in self.table):
				# oh. handy. we already did this dude: he was just
				# wanted multiple times.
				stack.pop()
				continue
			alldeps = self.get_dependencies(key)
			timing.phase("collapser loop # %s get_deps (%d) self %s" % (loop_count, len(alldeps), self.__class__))

#			def study(table):
#				keys = table.keys()
#				keys.sort()
#				hist = {}
#				for k in keys:
#					h = hash(k)
#					if (h not in hist):
#						hist[h] = []
#					hist[h].append(k)
#				def by_len(a,b):
#					return cmp(len(a), len(b))
#				v = hist.values()
#				v.sort(by_len)
#				v = v[::-1]
#				print "%d keys, %d unique hashes, worst duplication: %s" % (
#					len(keys), len(hist), v[:10])
#				for k in v[0]:
#					print "%d %s" % (hash(k), k)
#				raise Exception("done")

			def notintable(d):
#				if (len(self.table)>4000):
#					study(self.table)
				return d not in self.table
			newdeps = filter(notintable, alldeps)
			if (newdeps==[]):
				stack.pop()
				assert(key not in self.table)
				self.table[key] = self.collapse_impl(key)
				timing.phase("collapser loop # %s collapse_impl" % loop_count)
			else:
				stack += newdeps
			timing.phase("collapser loop # %s end" % loop_count)
		timing.phase("done")
#		print "collapser loop_count: %d" % loop_count
		return self.table[key]

	def lookup(self, key):
		# upcall from impl getting collapsed to find his dependencies' values
		return self.table[key]
