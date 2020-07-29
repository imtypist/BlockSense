#!/usr/local/bin/python
# Some useful pydot examples at:
# http://pythonhaven.wordpress.com/tag/pydot/
# GraphVis docs: http://www.graphviz.org/pdf/dotguide.pdf
# DOT language spec: http://www.graphviz.org/content/dot-language

import argparse
import re
import pydot

class Wire():
  def __init__(self, myid):
    self.myid = myid
    self.dests = []
    self.src = None
    self.val = None
    #print "Created a wire with ID <%s>" % myid

  def set_src(self, src):
    assert self.src == None # Shouldn't set this more than once
    self.src = src

  def add_dest(self, dest):
    self.dests += [dest]

  def set_val(self, val):
    assert self.src == None # Shouldn't set this more than once
    self.val = val

class IoWire(Wire):
  def __init__(self, myid, inputP):
    Wire.__init__(self, myid)
    self.inP = inputP 

class Gate():
  Colors = {"black":False, "blue":False, "forestgreen":False, "cadetblue2":False, "chartreuse":False, "darkorchid":False, "deeppink":False, "gold":False, "gray51":False, "greenyellow":False, "indianred":False, "plum":False}
  GateColors = {}
  GateIdCtr = 0

  def __init__(self, gtype, in_a, out_a):
    self.gtype = gtype
    self.in_a = in_a
    self.out_a = out_a
    self.inputWires = []
    self.outputWires = []
    self.node = None
    self.myid = Gate.get_id()

  def get_node(self):
    color = Gate.get_color(self.gtype)
    self.node = pydot.Node(self.gtype + ("%d" % self.myid), label=self.gtype, shape="invhouse", style="filled", fillcolor=color)
    return self.node

  @classmethod
  def get_id(cls): 
    cls.GateIdCtr += 1
    return cls.GateIdCtr

  @classmethod
  def get_color(cls, gtype):
    if gtype in cls.GateColors:
      return cls.GateColors[gtype]
    
    # Pick a new, unused color
    for color,used in cls.Colors.iteritems():
      if used:
        continue
      # Else, we've found an unused color to use
      cls.Colors[color] = True
      cls.GateColors[gtype] = color
      return color

    assert False  # Failed to find an unused color!

class ConstMulGate(Gate):
  def __init__(self, gtype, in_a, out_a, const):
    Gate.__init__(self, gtype, in_a, out_a)
    self.const = const 

  def get_node(self):
    color = Gate.get_color(self.gtype)
    self.node = pydot.Node(self.gtype + ("%d" % self.myid), label = ("* %d" % self.const), shape="invtriangle", style="filled", fillcolor=color)
    return self.node

class CircuitParser():
  def __init__(self):
    self.gates = []
    self.wires = {}
    self.iowires = [] 

  def add_gate(self, g):
    self.gates += [g]
  
  def add_wire(self, wire):
    self.wires[wire.myid] = wire

  def add_iowire(self, wire):
    self.iowires += [wire]
    self.add_wire(wire)

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

  def sub_parse(self, verb, in_a, out_a):
    if (verb=='split'):
      self.add_gate(Gate("split", in_a, out_a))
    elif (verb=="add"):
      self.add_gate(Gate("add", in_a, out_a))
    elif (verb=="mul"):
      self.add_gate(Gate("mul", in_a, out_a))
    elif (verb.startswith("const-mul-")):
      if (verb[10:14]=="neg-"):
        sign = -1
        value = verb[14:]
      else:
        sign = 1
        value = verb[10:]
      const = int(value, 16) * sign
      self.add_gate(ConstMulGate("const-mul", in_a, out_a, const))
    else:
      assert(False)

  def parse_line(self, line):
    verbsplit = filter(lambda t: t != '', line.split(' ', 1))
    if (len(verbsplit)<1):
      # blank line.
      return
    (verb,rest) = verbsplit
    #print "words: %s" % words
    if (verb=='total'):
      self.total_wires = int(rest)
    elif (verb=='input'):
      wire_id = int(rest)
      self.add_iowire(IoWire(inputP=True, myid = wire_id))
    elif (verb=='output'):
      wire_id = int(rest)
      self.add_iowire(IoWire(inputP=False, myid = wire_id))
    else:
      (in_a,out_a) = self.parse_io_list(rest)
      self.sub_parse(verb, in_a, out_a)

  def parse(self, arith_fn, wire_fn):
    self.total_wires = None  # not so important now that every wire is represented
    ifp = open(arith_fn, "r")
    for line in ifp.readlines():
      line = line.rstrip()
      noncomment = line.split("#")[0]
      self.parse_line(noncomment)
    ifp.close()

    # Convert each gate's list of wire IDs into actual Wire objects
    for gate in self.gates:
      for wireId in gate.in_a:
        wire = self.wires.get(wireId, None)
        if (wire == None):
          wire = Wire(wireId)
          self.wires[wireId] = wire
        gate.inputWires += [wire]
      for wireId in gate.out_a:
        wire = self.wires.get(wireId, None)
        if (wire == None):
          wire = Wire(wireId)
          self.wires[wireId] = wire
        gate.outputWires += [wire]
    
    if not wire_fn == None:
      ifp = open(wire_fn, "r")
      for line in ifp.readlines():
        line = line.strip()
        tokens = line.split()
        wireId = int(tokens[0])
        wireVal = int(tokens[1],16)

        self.wires[wireId].set_val(wireVal)
      ifp.close()

class CircuitRenderer():
  def __init__(self):
    pass

  def draw(self, circ, out_file):
    graph = pydot.Dot(graph_type='digraph')   # Digraph = Directed graph

    # Create nodes for each gate and each circuit-level I/O wire
    for wire in circ.iowires:
      node = None
      if wire.inP:
        node = pydot.Node("In %d" % wire.myid, shape="circle", style="filled", fillcolor="green")
        wire.set_src(node)
      else:
        node = pydot.Node("Out %d" % wire.myid, shape="circle", style="filled", fillcolor="red")
        wire.add_dest(node)
      graph.add_node(node)

    for gate in circ.gates:
      node = gate.get_node()
      graph.add_node(node)
      for wire in gate.inputWires:
        wire.add_dest(node)
      for wire in gate.outputWires:
        wire.set_src(node)

    # Convert the wires into edges
    invisibleNodeCount = 0
    for wire in circ.wires.values():
      src = wire.src
      if src == None:   # Make the script robust against missing src
        src = pydot.Node(invisibleNodeCount, style="invisible")
        invisibleNodeCount += 1

      for dest in wire.dests:
        label = "#%d" % wire.myid
        if (not wire.val == None):
          label += " = %d" % wire.val
        edge = pydot.Edge(src, dest, label=label)
        graph.add_edge(edge)

      if len(wire.dests) == 0:
        # Handle wires without destinations with an invisible node
        dest = pydot.Node(invisibleNodeCount, style="invisible")
        invisibleNodeCount += 1
        label = str(wire.myid)
        if (not wire.val == None):
          label += " = %d" % wire.val
        edge = pydot.Edge(src, dest, label=label)
        graph.add_node(dest)
        graph.add_edge(edge)

    #graph.write_png(out_file)
    #graph.write(out_file)
    graph.write_pdf(out_file)

def main(): 
  parser = argparse.ArgumentParser(description='Draw a circuit')
  parser.add_argument('--arith', dest='arith_file', required=True,
      help='arithmetic circuit description')
  parser.add_argument('--out', dest='out_file', required=True,
      help='file name for resulting graph')
  parser.add_argument('--wires', dest='wire_file', default=None,
      help='file containing the values each wire takes on')

  args = parser.parse_args()

  circ = CircuitParser()
  circ.parse(args.arith_file,args.wire_file)

  drawer = CircuitRenderer()
  drawer.draw(circ, args.out_file)


if (__name__=="__main__"):
  main()

