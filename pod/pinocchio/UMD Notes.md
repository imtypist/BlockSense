## Notes for getting started

The first goal is to be able to build and run the examples included in the Pinocchio distribution. We'll start with the matrix example.

In order to build the matrix example, we had to make the following changes: 

1. `eq-test.c` needs to be removed from `../common/App.py`
2. If compiling on a 64-bit machine, then `make.in` and `src/build_test_matrix.py` must be changed to use 32-bit gcc. Add `"gcc ... -m32 ..."` to these files every 'gcc' shows up. 
3. Run `../src/build-test-matrix.py` from `PWD=pinocchio/compiler/input`

### Example command for compiling C to a circuit (also see the makefiles)

```
python ~/v0.4/ccompiler/src/vercomp.py test/lh_test.c --il test/build/lh_test.c.il --arith test/build/lh_test.c.p.arith --ignore-overflow False --progress True
```

### Example of compiling a circuit to a SNARK, constructing a proof, verifying a proof
```

./pinocchio-v0.4.exe --qap --dv --file ccompiler/input/build/your_circuit.arith --input your_input_file --mem 4
```

This command shows all time data such as key generation time and proof generation time etc. 

```
./pinocchio-v0.4.exe --unit --dv --file ccompiler/input/build/your_circuit.arith --input your_input_file --output your_output_file
```

This command generates the output file. 

You can choose from qap or qsp, dv or pv etc. as listed in the help manual. 

In the input file, list all wireID wirevalue of inputs in order in hex value, and a const-1 input wire at the end. 
e.g `struct input{ a[2](2);} ` and let `a[0](0)=10 `, `a[1](1)=11 `, then the input file is 
```
0 a 
1 b 
2 1
```
## Dependencies
* Python
	* the "pycparser" parser library must be instaled
* Windows/Cygwin: 
	* Install 64-bit cygwin as well as python 2.7 and 32-bit toolchain packages Cygwin32-*
	* Add "CC=i686-PC-cygwin-gcc.exe" to the makefile
	* Remove the `matrix-test` file (or run `make clean`?)

## General Tips from James Litton

You should read the paper and any documentation included in Pinocchio itself. It is sparse, but details a few things like a few of the limits of the compiler and its entry and exit points. Also, Pinocchio was a work in progress.

When I ran it, it was a two-step process.

You took your C program and generated a circuit. Once you have a circuit (it can be tricky to write a C program that can compile to a circuit), you can use that as input to the compiler. In their source code they have some make files. I think you had to run `/ccompiler/src/build-test-matrix` and then there are makefiles in `/ccompiler/input`.

`Vercomp.py` is the program that takes a C program and generates a circuit.  At the time I was working on it, the Pinocchio executable itself was called `pinocchio-v0.4`. They both take arguments. The Pinocchio executable has a help string which is, well, helpful. For the `vercomp.py` one, look at the makefiles and its source code.

In terms of development environment the python code can be run on linux. On Windows I used Cygwin for all of this stuff. Make sure you use the 64bit version of Cygwin though because Pinocchio needs a lot of
addressable memory.

### Providing Input 

Input is provided by a file where each consecutive integer is provided on a newline with the value in hex, like so:
```
0 0xa5c
1 0x3a7
2 0xb3b
3 0x8ed
```
I believe the output file is given as an option for `Pinocchio.exe` and is provided in the same format. These are to correspond to the `wire values`.

### Unsupported arithmetic operations

I don’t believe Pinocchio supports division or modulo operators. It has addition, subtraction, and bit operations (if I remember correctly). For some values, division or modulo operations can be done with these operations trivially (e.g., `x % 8 == x & 7`). Multiplication of any two integers can be done with just bit operators and addition. 

### Accessing non-constant indices
Accessing non-constant indices is tricky. Technically pinocchio does not allow you to access elements in an array without a non-constant index.
```
#define ARRAY_SIZE 10
int x[ARRAY_SIZE](ARRAY_SIZE);
// This fails in pinocchio
int readX(int i) {
   return x[i](i);
}
```
However, you can accomplish effectively the same thing by "multiplexing:"
```
int readX_multiplexed(int i) {
   int j, result;
   for (j = 0; j < ARRAY_SIZE; j++) {
      int t = x[j](j); // This is OK since j is constant when the loop is unrolled
      if (j == i) result = t;
   }
   return result;
}
```

I think the trick was something like accessing all elements in the array but then not making use of the
values for all of them, but it was problematic.

### Specifying bit width
It's often useful (for efficiency) to perform arithmetic operations (+,*, etc) on full-size 254-bit values (the size of the native group elements). By default, pinocchio matches the semantics of 32-bit and truncates results if they're larger than this. The command line option
```
vercomp.py ... --bit-width 254
```
makes arithmetic operation use the full size (smaller bit widths are possible too, larger ones are not)