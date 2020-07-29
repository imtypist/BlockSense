#pragma once

#include <list>
#include <vector>
#include <string>

#include "Types.h"
#include "GateMul2.h"
#include "Gate.h"
#include "Wire.h"
#include "GateAddN.h"
#include "Field.h"

class GateMul2;

using namespace std;

typedef list<Gate*> GateList;
typedef list<GateAddN*> AdderList;
typedef vector<GateMul2*> MulGateVector;

typedef vector<Wire*> WireVector;

class Circuit { 
public:
	Circuit(Field* field);
	Circuit(Field* field, const string filename);
	~Circuit();

	// Keep all of these values explicitly, 
	// since these are the ones the QAP cares about
	WireVector inputs;			// Input values
	WireVector witness;			// Prover's secret witness inputs (a subset of the inputs vector)
	WireVector intermediates;	// Value of the output of each multiplication gate in the interior of the circuit
	WireVector outputs;			// Value of the circuit's actual output wires
	WireVector wires;			// Holds all of the wires

	GateList gates;			// All of the gates in the circuit
	MulGateVector mulGates;	// Just the multiplication gates (a subset of the gates list); useful for constructing the QAP

	void parse(const string  filename);

	// Based on the currently configured inputs, derive an appropriate set of outputs
	// Set optimized to true to do a direct (rather than gate-by-gate) evaluation
	void eval(bool optimized);	
  
  int numTrueInputs();  // Number of non-witness inputs

	Field* field;  // Field this circuit computes over
protected:
	
	WireVector nonMultIntermediates;	// Temporary storage for non-mult gate outputs (e.g., results of adders)

	virtual void directEval();
	virtual void gateEval();
	void addMulGate(GateMul2* gate);
	void addNonMulGate(Gate* gate);
	Wire* freshWire(WireVector& vector);
	// Generate num new wires and add them to vector.  Returns index of first new wire.
	int freshWires(WireVector& vector, int num);

	void connectInputs(Gate* gate, char* inputStr);
	void connectOutputs(Gate* gate, char* outputStr);
	void parseGate(char* type, int numGateInputs, char* inputStr, int numGateOutputs, char* outputStr);

	void assignIDs();
	void sort();	// Topologically sort the gates	
};