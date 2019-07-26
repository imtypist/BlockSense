#!/usr/bin/python
import argparse
import re
import os
import sys
mypath = os.path.dirname(__file__)
sys.path.append("%s/../../common" % mypath)
from App import App


class AppData:
  def __init__(self, app_name, param, bitwidth, filename):
    self.app_name = app_name
    self.param = param
    self.bitwidth = bitwidth
    self.muls = 0
    self.splits = 0
    self.splitMuls = 0
    self.degree = 0
    self.size = 0
    self.inputs = 0

    regex = re.compile("split in +([0-9]*) +<([0-9 ]*)> +out +([0-9]*) +<([0-9 ]*)>")

    ifp = open(filename, "r")
    for line in ifp:
      if line.startswith("mul"):
        self.muls += 1
      elif line.startswith("input"):
        self.inputs += 1
      else:
        result = regex.search(line)
        if not result == None:
          self.splits += 1
          self.splitMuls += int(result.group(3))+1
    ifp.close()

    self.degree = self.muls + self.splitMuls
    self.size = self.inputs + self.degree - self.splits # Subtract 1 per split parent


  def __str__(self):
    str = "%s-p%s-b%s:" % (self.app_name, self.param, self.bitwidth)
#    padding = 30 - len(str)
#    str += " "*padding
#    str += "%d\t%d" % (self.degree, self.size)
#    return str
    return '{:<30}{:10}{:10}'.format(str, self.degree, self.size)

class Counter:
  def __init__(self, dir):
    apps = App.defaultApps()
    appData = {}

    print '{:<30}{:^10}{:^10}'.format("Name", "Degree", "Size")
    #print "Name" + " "*26 + "Degree\tSize"
    for app in apps:
      for param in app.params:
        for bitwidth in app.bitwidths:
          app_base = "%s-p%s-b%s" % (app.name, param, bitwidth)
          filename = dir + "/" + app_base + ".arith"
          if (os.path.isfile(filename)):
            appData[app_base] = AppData(app.name, param, bitwidth, filename)
            print appData[app_base]
          else:
            print "%s:\t Missing" % (app_base)


      

def main(): 
  parser = argparse.ArgumentParser(description='Draw a circuit')
  parser.add_argument(dest='dir', 
      help='directory with arithmetic circuit descriptions')

  args = parser.parse_args()
  
  count = Counter(args.dir)

if (__name__=="__main__"):
  main()

