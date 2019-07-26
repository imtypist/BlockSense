#pragma once

#include <list>
#include <vector>
#include <string>

#include "Types.h"
#include "BoolGate.h"
#include "BoolWire.h"

class BoolWire;

using namespace std;

typedef std::list<BoolGate*> BoolGateList;
typedef std::vector<BoolWire*> BoolWireVector;

class BoolCircuit { 
public:
	BoolCircuit() { ; } 
	BoolCircuit(const string filename);
	~BoolCircuit();

	// Keep all of these values explicitly, 
	// since these are the ones the QSP cares about
	BoolWireVector inputs;			// Input wires
	BoolWireVector intermediates;	// Wires in the interior of the circuit
	BoolWireVector outputs;			// The circuit's actual output wires
	BoolWireVector wires;			// Pointers to all of the circuit's wires
	BoolWireVector ioWires;			// Pointers to all of the circuit's input/output wires

	BoolGateList gates;			// All of the gates in the circuit	

	// Based on the currently configured inputs, derive an appropriate set of outputs
	// Set optimized to true to do a direct (rather than gate-by-gate) evaluation
	void eval();	

protected:
	void addGate(BoolGate* gate);
	BoolWire* freshBoolWire(BoolWireVector& vector);

	// Generate num new wires and add them to vector.  Returns index of first new wire.
	int freshBoolWires(BoolWireVector& vector, int num);

	void parse(const string filename);
	BoolGate* parseGate(char* type, int numGateInputs, char* inputStr, int numGateOutputs, char* outputStr);

	void assignIDs();
	virtual void consolidate();		// Create some helpful data structures to facilitate later operations
	// Topologically sort the gates
	void sort();
};