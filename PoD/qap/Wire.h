#pragma once

#include <list>

#include "Types.h"
#include "Gate.h"
#include "Field.h"

class Wire;
class Gate;

typedef std::list<Wire *> WireList;

class Wire {
public:
	Wire(Gate* in = NULL, Gate* out = NULL) : input(in), output(out) { visited = false; Field::Zero(&value); trueInput = false; trueOutput = false; witnessInput = false; }
	
	Gate* input;	// NULL if this is a circuit input
	Gate* output;	// NULL if this is a circuit output
	FieldElt value;
	bool visited;	// Used for keeping track of our progress when traversing the circuit
	bool trueInput;	// Verified input to the circuit
	bool witnessInput;	// An input that's part of the prover's secret witness
	bool trueOutput;	// Verified output from the circuit
	long long id;
};

