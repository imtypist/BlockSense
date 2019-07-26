import argparse
import sys
import re
sys.setrecursionlimit(60)

from Collapser import Collapser

class BaseEvalGate:
	def __init__(self):
		pass

	def eval(self, ae):
		return self.eval_internal(ae)

class BaseEvalInput(BaseEvalGate):
	def __init__(self, wire_id):
		BaseEvalGate.__init__(self)
		self.wire_id = wire_id

	def inputs(self):
		return []

	def eval_internal(self, ae):
		return ae.get_input(self.wire_id)

class BaseEvalOp(BaseEvalGate):
	def __init__(self, wire_list, pyop):
		BaseEvalGate.__init__(self)
		self.wire_list = wire_list
		self.pyop = pyop

	def inputs(self):
		return self.wire_list

	def eval_internal(self, ae):
		input_args = map(ae.lookup, self.wire_list)
		return reduce(self.pyop, input_args)

##############################################################################

class BaseEval(Collapser):
	def __init__(self):
		Collapser.__init__(self)
		self.gates = {}
		self.outputs = set()

		parser = argparse.ArgumentParser(description='Evaluate circuit')
		parser.add_argument('circuitfile', metavar='<circuitfile>',
			help='the circuit file')
		parser.add_argument('infile', metavar='<infile>',
			help='the input I/O file')
		parser.add_argument('outfile', metavar='<outfile>',
			help='the output I/O file')
		parser.add_argument('--wires', dest='wirefile',
			help="record intermediate wire values in <wirefile>")

		args = parser.parse_args()

		self.parse(args.circuitfile)
		self.load_inputs(args.infile)
		self.evaluate(args.outfile)
		self.fill_gaps()
		if (args.wirefile):
			self.write_wires(args.wirefile)

	def sub_args(self, parser): raise Exception("abstract")
	def sub_parse(self, verb, in_a, out_a): raise Exception("abstract")

	def unique(self, array):
		assert(len(array)==1)
		return array[0]

	def parse_line(self, line):
		verbsplit = filter(lambda t: t != '', line.split(' ', 1))
		if (len(verbsplit)<1):
			# blank line.
			return
		(verb,rest) = verbsplit
		#print "words: %s" % words
		if (verb=='total'):
			if (self.total_wires!=None):
				raise Exception("Duplicate total line")
			self.total_wires = int(rest)
		elif (verb=='input'):
			wire_id = int(rest)
			self.add_gate(wire_id, BaseEvalInput(wire_id))
		elif (verb=='output'):
			self.outputs.add(int(rest))
		else:
			(in_a,out_a) = self.parse_io_list(rest)
			self.sub_parse(verb, in_a, out_a)

	def parse(self, arith_fn):
		self.total_wires = None	# not so important now that every wire is represented
		ifp = open(arith_fn, "r")
		for line in ifp.readlines():
			line = line.rstrip()
			noncomment = line.split("#")[0]
			self.parse_line(noncomment)
		ifp.close()

		# verify a couple properties
		# all the outputs exist
		for w in self.outputs:
			assert(w in self.gates)

		# no wires number greater than the total_wires
		wire_ids = self.gates.keys()
		wire_ids.sort()
		max_wire = wire_ids[-1]
		assert(max_wire+1==self.total_wires)

		# all wires should have been labeled.
		for wire_id in range(self.total_wires):
			if (wire_id not in self.gates):
				print "%s not in gates" % wire_id
				assert(False)

	def load_inputs(self, in_fn):
		self.inputs = {}
		ifp = open(in_fn, "r")
		for line in ifp.readlines():
			(wire_id_s,value_s) = line.split(' ')
			wire_id = int(wire_id_s)
			value = int(value_s, 16)
			#print "setting %s" % wire_id
			self.inputs[wire_id] = value
			#print "loaded %d with %x" % (wire_id, value)
		ifp.close()

		for (wire_id,gate) in self.gates.items():
			if (isinstance(gate, BaseEvalInput)):
				if (wire_id not in self.inputs):
					print "No value assigned to Input %s" % wire_id
					assert(False)

	def get_input(self, wire):
		return self.inputs[wire]

	def evaluate(self, out_fn):
		ofp = open(out_fn, "w")
		output_values = {}
		output_ids = list(self.outputs)
		output_ids.sort()
		for wire_id in output_ids:
			ofp.write("%d %x\n" % (wire_id, self.collapse_tree(wire_id)))
		ofp.close()

	def fill_gaps(self):
		wire_ids = self.table.keys()	# Note peek into superclass' member. :v(
		wire_ids.sort()
		max_wire_id = wire_ids[-1]
		full_set = set(range(0, max_wire_id))
		missing_wire_ids = list(full_set - set(wire_ids))
		missing_wire_ids.sort()
		for wire_id in missing_wire_ids:
				self.collapse_tree(wire_id)
    
	def write_wires(self, wire_fn):
		wire_fp = open(wire_fn, "w")
		wire_ids = self.table.keys()	# Note peek into superclass' member. :v(
		wire_ids.sort()
		next_wire_id = 0
		for wire_id in wire_ids:
			if wire_id > next_wire_id:
				for i in range(next_wire_id, wire_id):
					wire_fp.write("%d 0x0  # Unevaluated\n" % i)
					assert(False)
			wire_fp.write("%d %s\n" % (wire_id, hex(int(self.lookup(wire_id))).replace("L", "")))
			next_wire_id = wire_id+1
		wire_fp.close()

	def get_dependencies(self, wire):
		deps = self.gates[wire].inputs()
		if (len(deps)>0 and not (max(deps) < wire)):
			raise Exception("arith file doesn't flow monotonically: deps %s come after wire %s" % (deps, wire))
		return deps

	def collapse_impl(self, wire):
		return self.gates[wire].eval(self)
	
# DEAD CODE
#	def eval_nonrecursive(self, root_wire_id):
#		# well, it's still recursive, but we maintain our own stack.
#		stack = [root_wire_id]
#		stack_set = None
#		#stack_set = set(stack)
#			# A set is not a good test for loops, since a dag can
#			# trigger it.
##		max_depth = 0
#		while (len(stack)>0):
##			max_depth = max(len(stack), max_depth)
#			w = stack[-1]
#			if (w in self.values):
#				stack.pop()
#				if (stack_set!=None):
#					stack_set.remove(w)
#			else:
#				subwires = self.gates[w].inputs()
#				unknown_subwires = filter(lambda w: w not in self.values, subwires)
#				if (len(unknown_subwires)==0):
#					self.values[w] = self.gates[w].eval(self)
#					stack.pop()
#					if (stack_set!=None):
#						stack_set.remove(w)
#				else:
#					if (not (max(unknown_subwires) < w)):
#						print "w %s max unknown %s" % (w, max(unknown_subwires))
#						print "unknown: %s" % unknown_subwires
#						raise Exception("arith file doesn't flow monotonically.")
#					if (stack_set!=None):
#						for uw in unknown_subwires:
#							if (uw in stack_set):
#								print "Inserting dup %s trying to service %s"%(
#									uw, w)
#								print "stack: %s" % stack
#								assert(uw not in stack_set)
#							stack_set.add(uw)
#							stack.append(uw)
#					else:
#						stack += unknown_subwires
#
##		print "max depth %d" % max_depth
#		return self.values[root_wire_id]

	def add_gate(self, wire_id, gate):
		#print "add_gate %d %s" % (wire_id, gate)
		if (wire_id in self.gates):
			raise Exception("duplicate wire %s" % wire_id)
			assert(wire_id not in self.gates)
		self.gates[wire_id] = gate

	def parse_io_list(self, io_str):
		mo = re.compile("in +([0-9]*) +<([0-9 ]*)> +out +([0-9]*) +<([0-9 ]*)>").search(io_str)
		if (mo==None):
			raise Exception("Not an io list: '%s'" % io_str)
		things = mo.groups(0)
		in_l = int(things[0])
		in_a = map(int, things[1].split())
		#print "in_l %s in_a %s" % (in_l, in_a)
		assert(len(in_a)==in_l)
		out_l = int(things[2])
		out_a = map(int, things[3].split())
		assert(len(out_a)==out_l)
		return (in_a, out_a)
		
# DEAD CODE
#	def parse_wire_list(self, promised_count, list_s):
#		#print "list_s = %s" % list_s
#		mo = re.compile("<([0-9 ]*)>").search(list_s)
#		wires_s = mo.groups(1)[0].split()
#		wires_i = map(int, wires_s)
#		return wires_i
