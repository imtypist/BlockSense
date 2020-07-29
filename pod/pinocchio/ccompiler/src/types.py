#!/usr/bin/python
import os
import sys
mypath = os.path.dirname(__file__)
sys.path.append("%s/../external-code/ply-3.4" % mypath)
sys.path.append("%s/../external-code/pycparser-2.08" % mypath)
import pycparser

#int i;
#float f;
#float f[10];
#struct s_foo { int a; int b; } bar;
#struct s_foo c;
#struct s_foo c[10];
#struct s_foo3 { int array[10]; };
#struct foo { int f1; int f2; };
#struct foo f;
#struct baz { int b1; int b2; } b;
#void foo(int a, int *b, struct foo *c) {}

test = """
int a[7];
int main() {
a[1] = 2;
}
"""
# int i is an ArrayVal
# float f[10] is an ArrayRef
# struct s_foo c; is an ArrayVal(Struct): you can't evaluate it, only . or &
# struct s_foo c[10]; is an ArrayRef. When you evaluate it, you have a
# ArrayVal(Struct).

parser = pycparser.CParser()
ast = parser.parse(test)
#print ast.show()
for (name, child) in ast.children():
	try:
		print
		print child.show()
		print child.__dict__
		print child.decl.__dict__
		print child.decl.type.__dict__
		print child.decl.type.args.__dict__
		for param in child.decl.type.args.params:
			print param.__dict__
	except: pass

	try:
		print child.body.__dict__
		print child.body.block_items[0].lvalue.__dict__
		print child.body.block_items[0].lvalue.name.__dict__
	except: pass
