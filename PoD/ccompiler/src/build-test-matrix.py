#!/usr/bin/python

import subprocess
import os
import sys
mypath = os.path.dirname(__file__)
sys.path.append("%s/../../common" % mypath)
from App import App

BUILD="build/"
GCC="gcc -g -I../../common -Ibuild/".split(' ')

def dash_to_under(s):
	if (s[0]=="-"):
		s = "_"+s[1:]
	return s

class OneLiner:
	def __init__(self, line):
		self.line = line

	def emit(self):
		return self.line

class Comment(OneLiner):
	def __init__(self, line):
		OneLiner.__init__(self, line)

class Directive(OneLiner):
	def __init__(self, line):
		OneLiner.__init__(self, line)

class Make:
	def __init__(self, output, inputs, cmdlist):
		self.output = output
		self.inputs = inputs
		self.cmdlist = cmdlist

	def emit(self):
		if (len(self.inputs) > 3):
			dependencies = " \\\n\t".join(self.inputs)
		else:
			dependencies = " ".join(self.inputs)

		if (self.cmdlist==[]):
			rule = ""
		else:
			rule = "	%s\n" % " ".join(self.cmdlist)

		return "%s: %s\n%s\n" % (
			self.output,
			dependencies,
			rule)

class BuildTestMatrix:
	def __init__(self):
		apps = App.defaultApps() 
		self.make_rules = []
		self.precious_targets = []
		self.random_headers_created = set()

		self.make_random_header = BUILD+"make-random-header"
# obsoleted by include
#		output = self.make_random_header
#		inputs = [ "make-random-header.c" ]
#		self.make(output, inputs,
#			[ "gcc", "-o", output ] + inputs)

		#self.system(["make"])	# be sure common .os are built
		for app in apps:
			self.make_rules.append(Comment("# App %s\n" % app.name))
			for param in app.params:
				self.make_rules.append(Comment("#   Param %s\n" % param))
				for bitwidth in app.bitwidths:
					self.make_rules.append(Comment("#     Bitwidth %s\n" % bitwidth))
					self.build(app, param, bitwidth)

		all_rule = Make("matrix-all", ["build"]+self.precious_targets, [])
		self.make_rules = [all_rule] + self.make_rules
		self.make_rules.append(Directive("include make.in\n"))

		makefp = open("make.matrix", "w")
 		makefp.write("PYTHON=python\n\n")
		for makerule in self.make_rules:
			makefp.write(makerule.emit())
		makefp.close()

	def make(self, output, inputs, cmdlist):
		self.make_rules.append(Make(output, inputs, cmdlist))

	def precious(self, target):
		self.precious_targets.append(target)

	def get_random_config(self, app, param):
		gcc = subprocess.Popen([
			"gcc",
			"-DQUERY_RANDOM_CONFIG",
			"-DPARAM=%d" % param,
			"-E",
			"%s.c" % app.name], stdout=subprocess.PIPE)
		grep = subprocess.Popen(["grep", "pragma message"],
			stdin=gcc.stdout, stdout=subprocess.PIPE)
		gcc.stdout.close()
		text = grep.communicate()[0]
		grep.stdout.close()
		lines = text.split('\n')
		config = {}
		for line in lines:
			fields = line.split()
			if (len(fields)<4):
				continue
			var = fields[2].replace("'", "")
			value = eval(fields[3])
			config[var] = value
		return config

	def add_random_header(self, random_config):
		random_header = BUILD+"random-header-%d-%d.h" % (
			random_config["RANDOM_SIZE"], random_config["RANDOM_REDUCE"])
		if (random_header not in self.random_headers_created):
			output = random_header
			inputs = [ self.make_random_header ]
			self.make(output, inputs,
				[ self.make_random_header,
					output,
					str(random_config["RANDOM_SIZE"]),
					str(random_config["RANDOM_REDUCE"]) ])
			self.random_headers_created.add(random_header)
		return random_header

	def build(self, app, param, bitwidth):
		random_config = self.get_random_config(app, param)
		needs_random = "RANDOM_SIZE" in random_config

		app_base = "%s-p%s-b%s" % (app.name, param, bitwidth)
		DPARAM = "-DPARAM=%d" % param
		DBIT_WIDTH = "-DBIT_WIDTH=%d" % bitwidth
		random_header_cpparg = []
		rh_cpparg = [ ]

		if (bitwidth==32):
			qsp_o = BUILD+("qsp-test-%s-p%s.o" % (app.name, param))

			if (needs_random):
				# make the requisite .h
				random_header = self.add_random_header(random_config)
				rh_input_list = [ random_header ]
				rh_include_list = [ "-include", random_header ]
				rh_cpparg = [ dash_to_under("-include"), random_header ]
			else:
				random_header = None
				rh_input_list = [ ]
				rh_include_list = [ ]
				rh_cpparg = [ ]

			# build the C executable module
			output = BUILD+app_base+"-native.o"
			c_file = app.name+".c"
			inputs = [ c_file ] + rh_input_list
			self.make(output, inputs,
				GCC+["-c", "-o", output, c_file]+rh_include_list+["-DQSP_TEST", DPARAM, DBIT_WIDTH])

			# build the C test module
			output = BUILD+app_base+"-test.o"
			inputs = [ app.name+"-test.c" ]
			self.make(output, inputs,
				GCC+["-c", "-o", output]+inputs+["-DQSP_TEST", DPARAM, DBIT_WIDTH])

			# param-specific qsp module
			output = qsp_o
			inputs = ["qsp-test.c"]
			self.make(output, inputs,
				GCC+["-c", "-o", output]+inputs+[DPARAM])

			# link the C program
			output = BUILD+app_base
			inputs = [ BUILD+app_base+"-native.o",
						BUILD+app_base+"-test.o",
						qsp_o,
						BUILD+"wire-io.o",
						BUILD+"print-matrix.o" ]
			self.make(output, inputs,
				GCC +["-o", output] + inputs)
			self.precious(output)

		moreflags = []
		if (app.ignore_overflow):
			moreflags += ["--ignore-overflow", "True"]

		output = BUILD+app_base+".arith"
		inputs = [ app.name+".c" ]
		self.make(output, inputs,
			["$(PYTHON) ../src/vercomp.py"]+inputs+[
				"--arith", output,
				"--bit-width", str(bitwidth),
				"--cpparg"]
				+map(dash_to_under, [
					"-Ibuild/", DPARAM, DBIT_WIDTH
					]+rh_cpparg)
				+moreflags
				)
		self.precious(output)


BuildTestMatrix()
