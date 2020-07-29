#!/usr/bin/python

# TODO: Condense adds of the same wire into Const-Muls

#import cProfile
import argparse
import re
from collections import deque, Counter

class Wire():
  def __init__(self, myid, comment=""):
    self.myid = myid
    self.dests = Counter()
    self.src = None
    self.comment = comment
    self.dead = False

  def set_src(self, src):
    assert self.src == None # Shouldn't set this more than once
    self.src = src

  def add_dest(self, dest):
    self.dests[dest] += 1

  def remove_dest(self, dest):
    self.dests[dest] -= 1

  def __str__(self):
    str = "ID: %d.  From: [%s]. To: " % (self.myid, self.src)
    for dest in self.dests:
      str += "[%s], " % dest
    return str
    
class IoWire(Wire):
  def __init__(self, myid, inputP, comment):
    Wire.__init__(self, myid, comment)
    self.inP = inputP 

class Gate():
  GateIdCtr = 0

  def __init__(self, gtype, in_a, out_a, comment):
    self.gtype = gtype
    self.in_a = in_a
    self.out_a = out_a
    self.comment = comment
    self.inputWires = deque()
    self.outputWires = deque()
    self.dead = False
    self.myid = Gate.get_id()

  def wireStr(self, wires):
    str = ""
    for wire in wires:
      str += "%d " % wire.myid 
    return str.rstrip()

  def type_str(self): 
    return self.gtype

  def add_input(self, wire):
    self.inputWires.append(wire)
  
  def add_output(self, wire):
    self.outputWires.append(wire)
    
  def __str__(self):
    type_str = self.type_str()
    ret = "%s in %d <%s> out %d <%s>" % (type_str,
      len(self.inputWires), self.wireStr(self.inputWires),
      len(self.outputWires), self.wireStr(self.outputWires))
    return ret 

  def __hash__(self):
    return hash(self.myid)

  @classmethod
  def get_id(cls): 
    cls.GateIdCtr += 1
    return cls.GateIdCtr

#class ConstMulGate(Gate):
#  def __init__(self, gtype, in_a, out_a, comment, const):
#    Gate.__init__(self, gtype, in_a, out_a, comment)
#    self.const = const 
#
#  def type_str(self):
#    if const
#    return "const-mul-%s" % hex(self.const)[2:]

class CircuitParser():
  def __init__(self):
    self.gates = set()
    self.add2gates = deque()
    self.wires = {}
    self.iwires = [] 
    self.owires = [] 
    self.total_wires = None
    self.orig_total_wires = None

  def add_gate(self, g):
    self.gates.add(g)
    if g.gtype == "add": 
      self.add2gates.append(g)
  
  def add_wire(self, wire):
    self.wires[wire.myid] = wire

  def add_iowire(self, wire):
    if (wire.inP):
      self.iwires += [wire]
    else:
      self.owires += [wire]

    self.add_wire(wire)

  def condense(self, debug=False):
    condensedCtr = 0
    while len(self.add2gates) > 0:
      # (while is preferable to for loop, since we free up deque mem as we go)
      gate = self.add2gates.pop()   
      if gate.dead:
        continue

      # Make a copy so we can keep track of which ones we've processed
      gateInputs = deque(gate.inputWires)  # TODO: Do this via indexing?

      while len(gateInputs) > 0:
        inputWire = gateInputs.pop()
        parent = inputWire.src
#        if inputWire.myid == 5080:
#          print inputWire
#          print parent
        if parent != None and parent.gtype == "add" and (len(inputWire.dests) == 1):
          if debug:
            print "Merging %s with parent %s" % (gate, parent)
          # Merge the parent into gate by rewiring parent's inputs to be gate's inputs
          for parentInputWire in parent.inputWires:
            gateInputs.append(parentInputWire)
            gate.inputWires.append(parentInputWire)

            parentInputWire.remove_dest(parent)
            parentInputWire.add_dest(gate)
          if parent in self.gates:
             self.gates.remove(parent)
          condensedCtr += 1
          parent.dead = True

          # Remove the wire and update our total
          if not inputWire.dead:
            inputWire.dead = True
            self.total_wires -= 1
#          if inputWire.myid in self.wires.keys():
#            del(self.wires[inputWire.myid])
#            self.total_wires -= 1
          gate.inputWires.remove(inputWire)

    print "Collapsed %d gates.  Renumbering..." % condensedCtr

#    if (not self.total_wires == len(self.wires)):
#      print "Lost track of some wires!  Counted %d but have %d" % \
#        (self.total_wires, len(self.wires))

    if not debug:
      i = 0
      for wire in self.wires.values():
        if not wire.dead:
          wire.myid = i
          i += 1
      assert( i == self.total_wires )

  def add_comment(self, str, comment):
    comment_indent = 42
    ret = str + " " * (comment_indent - len(str)) + "#" + comment + "\n"
    return ret

  def write(self, out_file_name, debug=False): 
    file = open(out_file_name, "w")

    file.write("total %d\n" % self.total_wires)
#    if not debug:
#      file.write("total %d\n" % len(self.wires))
#    else:
#      file.write("total %d\n" % self.orig_total_wires)
    for wire in self.iwires:
      file.write(self.add_comment("input %d" % wire.myid, wire.comment))

    for gate in self.gates:
      file.write(self.add_comment("%s" % gate, gate.comment))
    
    for wire in self.owires:
      file.write(self.add_comment("output %d" % wire.myid, wire.comment))
  
    file.close()
  
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

  def sub_parse(self, verb, in_a, out_a, comment):
    self.add_gate(Gate(verb, in_a, out_a, comment))
#    if (verb=='split'):
#      self.add_gate(Gate("split", in_a, out_a, comment))
#    elif (verb=="add"):
#      self.add_gate(Gate("add", in_a, out_a, comment))
#    elif (verb=="mul"):
#      self.add_gate(Gate("mul", in_a, out_a, comment))
#    elif (verb.startswith("const-mul-")):
#      if (verb[10:14]=="neg-"):
#        sign = -1
#        value = verb[14:]
#      else:
#        sign = 1
#        value = verb[10:]
#      const = int(value, 16) * sign
#      self.add_gate(ConstMulGate("const-mul", in_a, out_a, comment, const))
#    else:
#      assert(False)

  def parse_line(self, line, comment):
    verbsplit = filter(lambda t: t != '', line.split(' ', 1))
    if (len(verbsplit)<1):
      # blank line.
      return
    (verb,rest) = verbsplit
    #print "words: %s" % words
    if (verb=='total'):
      self.orig_total_wires = int(rest)
      self.total_wires = int(rest)
    elif (verb=='input'):
      wire_id = int(rest)
      self.add_iowire(IoWire(inputP=True, myid = wire_id, comment=comment))
    elif (verb=='output'):
      wire_id = int(rest)
      self.add_iowire(IoWire(inputP=False, myid = wire_id, comment=comment))
    else:
      (in_a,out_a) = self.parse_io_list(rest)
      self.sub_parse(verb, in_a, out_a, comment)

  def parse(self, arith_fn):
    self.total_wires = None  # not so important now that every wire is represented
    ifp = open(arith_fn, "r")
    for line in ifp:
      line = line.rstrip()
      tokens = line.split("#")
      noncomment = tokens[0]
      comment = ""
      if (len(tokens) > 1):
        comment = tokens[1]
      self.parse_line(noncomment, comment)
    ifp.close()

    # Convert each gate's list of wire IDs into actual Wire objects
    print "Building Wire objects..."
    for gate in self.gates:
      for wireId in gate.in_a:
        wire = self.wires.get(wireId, None)
        if (wire == None):
          wire = Wire(wireId)
          self.wires[wireId] = wire
        gate.add_input(wire)
        wire.add_dest(gate)
      for wireId in gate.out_a:
        wire = self.wires.get(wireId, None)
        if (wire == None):
          wire = Wire(wireId)
          self.wires[wireId] = wire
        gate.add_output(wire)
        wire.set_src(gate)
    
def main(): 
  parser = argparse.ArgumentParser(description='Coallesce Add2 gates into AddN gates')
  parser.add_argument('--arith', dest='arith_file', required=True,
      help='arithmetic circuit description')
  parser.add_argument('--out', dest='out_file', required=True,
      help='file name for the updated circuit')
  parser.add_argument('--debug', action='store_true', 
      help='Take extra debugging precautions')

  args = parser.parse_args()

  circ = CircuitParser()
  circ.parse(args.arith_file)
  print "Parsing complete.  Condensing..."
  circ.condense(args.debug)
  circ.write(args.out_file, args.debug)


if (__name__=="__main__"):
  main()
  # View results with: ../src/profile_print.py condensed.prof 
  #cProfile.run("main()", "condensed.prof")

