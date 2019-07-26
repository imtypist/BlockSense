#include "GateMul2.h"
#include "Circuit.h"
#include "GateAddN.h"
#include "GateMulConst.h"
#include "GateAddConst.h"
#include "GateBitSplitMany.h"
#include "GateBitSel.h"
#include "wire-io.h"
#include "GateZerop.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <assert.h>
#include <list>
using namespace std;

Circuit::Circuit(Field* field) {
	this->field = field;
}

Circuit::Circuit(Field* field, const string filename) {
	this->field = field;
	parse(filename);

	this->sort();	// Topologically sort all of the gates we just added
	this->assignIDs();
}

void deleteWires(WireVector& wires) {
	for (WireVector::iterator iter = wires.begin();
		 iter != wires.end();
		 iter++) {
		delete (*iter);
	}
}


Circuit::~Circuit() {
	// Delete all of the gates
	while(!gates.empty()) {
		delete gates.front();
		gates.pop_front();
	}

	// Delete all of the wires
	deleteWires(inputs);
	deleteWires(intermediates);
	deleteWires(nonMultIntermediates);
	deleteWires(outputs);
}

void Circuit::directEval() {
	assert(0);
}

int Circuit::numTrueInputs() {
  return (int) (inputs.size() - witness.size());
}

void Circuit::eval(bool optimized) {
	if (optimized) {
		directEval();
	} else {
		gateEval();
	}
}

void Circuit::gateEval() {
	for (GateList::iterator iter = gates.begin();
		   iter != gates.end();
			 iter++) {
		(*iter)->eval();
	}
}


void Circuit::addMulGate(GateMul2* gate) {
	gates.push_back(gate);
	mulGates.push_back(gate);
}

void Circuit::addNonMulGate(Gate* gate) {
	gates.push_back(gate);
}

Wire* Circuit::freshWire(WireVector& vector) {
	int index = freshWires(vector, 1);
	return vector[index];
}

// Generate num new wires and add them to vector.  Returns index of first new wire.
int Circuit::freshWires(WireVector& vector, int num) {
	int oldSize = (int) vector.size();
	vector.resize(oldSize + num);

	for (int i = 0; i < num; i++) {
		vector[oldSize + i] = new Wire();
	}

	return oldSize;
}



// Topologically sort the gates
// TODO: Technically, addGate could be maintained as a heap to avoid this step
void Circuit::sort() {
	for (int i = 0; i < inputs.size(); i++) {
		inputs[i]->visited = true;
	}

	size_t numUnReady = gates.size();

	while (numUnReady > 0) {
		// Iterate through the unready circuits at the front of the list
		int numReady = 0;
		GateList::iterator iter = gates.begin();
		for (int i = 0; i < numUnReady; i++) {
			Gate* g = *iter; 
			if (g->testAndSetReady()) {
				// Move it to the back of the queue
				iter = gates.erase(iter);
				gates.push_back(g);
				numReady++;
			} else {
				// Leave it in place, but advance to the next item
				iter++;
			}			
		}
		numUnReady -= numReady;
	}		
}

// It is convenient to assign unique IDs to each of the inputs and multiplication gates
void Circuit::assignIDs() {
	for (int i = 0; i < inputs.size(); i++) {
		inputs[i]->id = i;
	}

	unsigned long polyId = 0;
	unsigned long rootId = 0;
	for (MulGateVector::iterator iter = mulGates.begin();
		iter != mulGates.end();
		iter++) {
		// Everyone gets a root ID
		(*iter)->rootId = rootId++;

		// Gates without output don't get a poly ID, since they don't need a polynomial
		if ((*iter)->polyCount() > 0) {
			assert((*iter)->polyCount() == 1);	// Don't currently handle more than 1
			// Needs at least one poly, so give it an ID
			(*iter)->polyId = polyId++;
		} else {
			// Doesn't need a poly ID so assign a bogus one we can identify when things go wrong
			(*iter)->polyId = -42;
		}

		assert(rootId != ULONG_MAX && polyId != ULONG_MAX);
	}

	for (int i = 0; i < wires.size(); i++) {
		wires[i]->id = i;
	}
}

#if 0
Gate* Circuit::parseGate(Wire* output, char* type, int numGateInputs, char* inputStr) {
	Gate* gate = NULL;
		
	if (strcmp(type, "add") == 0) {
		gate = new GateAddN(this->field, numGateInputs);
		addNonMulGate(gate);
	} else if (strcmp(type, "mul") == 0) {
		gate = new GateMul2(this->field);
		addMulGate((GateMul2*)gate);	
	/* 
		// Still need to figure out how to add the const to the polynomials (mod.add currently not used)
	} else if (strstr(type, "const-add-")) {
		int constant = 0;
		int ret = sscanf_s(type, "const-add-%d", &constant);
		assert(ret == 1);
		gate = new GateAddConst(this->field, constant);
		addNonMulGate(gate);
		*/
	} else if (strstr(type, "const-mul-")) {
		int constant = 0;
		int ret = sscanf_s(type, "const-mul-%d", &constant);
		assert(ret == 1);

		gate = new GateMulConst(this->field, constant);
		addNonMulGate(gate);
	} else {
		assert(0);	// Unexpected gate type
	}

	long wireId;	
	istringstream iss (inputStr,istringstream::in);
	while (iss >> wireId) {
		if (wireId < inputs.size()) {
			gate->assignInput(inputs[wireId]);
		} else {
			int intermediateIndex = (int) (wireId - inputs.size());	// Compute appropriate offset into intermediate wires
			assert(intermediateIndex >= 0 && intermediateIndex < intermediates.size());
			gate->assignInput(intermediates[intermediateIndex]);
		}
	}

	gate->assignOutput(output);	

	return gate;
}


/* Need some hash tables here, I guess :( */
void Circuit::parse(const string filename) {
	long wireId, numInputs, numOutputs, numIntermediates, numGateInputs;
	wireId = numInputs = numOutputs = numIntermediates = numGateInputs = 0;
	char type[200], inputStr[2000];

	ifstream ifs ( filename , ifstream::in );
	string line;
	getline(ifs, line);
	int ret = sscanf_s(line.c_str(), "%d %d %d", &numInputs, &numIntermediates, &numOutputs);
	assert(ret == 3);

	freshWires(this->inputs, numInputs);
	freshWires(this->outputs, numOutputs);
	freshWires(this->intermediates, numIntermediates);

	while (getline(ifs, line)) {
		if (4 == sscanf_s(line.c_str(), "%d gate %s inputs %d <%[^>]", &wireId, type, sizeof(type), &numGateInputs, inputStr, sizeof(inputStr))) {
			int intermediateIndex = wireId - numInputs;	// Compute appropriate offset into intermediate wires
			assert(intermediateIndex >= 0 && intermediateIndex < numIntermediates);
			parseGate(intermediates[intermediateIndex], type, numGateInputs, inputStr);
		} else if (4 == sscanf_s(line.c_str(), "%d output gate %s inputs %d <%[^>]", &wireId, type, sizeof(type), &numGateInputs, inputStr, sizeof(inputStr))) {
			int outputIndex = wireId - numInputs - numIntermediates;	// Compute appropriate offset into output wires
			assert(outputIndex >= 0 && outputIndex < numOutputs);
			parseGate(outputs[outputIndex], type, numGateInputs, inputStr);
		} else {
			printf("Error: Unrecognized line: %s\n", filename);
			assert(0);
		}
	}

	// We only want mulGate outputs in the intermediate list
	WireVector tmp;
	tmp.reserve(mulGates.size());		// Max this can possibly grow
	nonMultIntermediates.reserve(intermediates.size());
	for (int i = 0; i < intermediates.size(); i++) {
		if (intermediates[i]->input != NULL && intermediates[i]->input->isMult()) {
			tmp.push_back(intermediates[i]);
		} else {
			nonMultIntermediates.push_back(intermediates[i]);
		}
	}

	// Overwrite the intermediates with the true mulGate intermediates
	intermediates = tmp;
}
#endif

void Circuit::connectInputs(Gate* gate, char* inputStr) {
	// Connect all of the input wires
	long wireId;	
	istringstream iss (inputStr, istringstream::in);
	while (iss >> wireId) {
		gate->assignInput(wires[wireId]);
	}
}

void Circuit::connectOutputs(Gate* gate, char* outputStr) {
	// Connect all of the input wires
	long wireId;	
	istringstream iss (outputStr, istringstream::in);
	while (iss >> wireId) {
		gate->assignOutput(wires[wireId]);
	}
}

void Circuit::parseGate(char* type, int numGateInputs, char* inputStr, int numGateOutputs, char* outputStr) {
	Gate* gate = NULL;
		
	// Deduce the type
	if (strcmp(type, "add") == 0) {
		gate = new GateAddN(this->field, numGateInputs);
	
		// Connect all of the wires
		assert(numGateInputs >= 1);
		assert(numGateOutputs == 1);
		connectInputs(gate, inputStr);
		connectOutputs(gate, outputStr);

		addNonMulGate(gate);
	} else if (strcmp(type, "mul") == 0) {
		gate = new GateMul2(this->field);

		// Connect all of the wires
		assert(numGateInputs == 2);
		assert(numGateOutputs == 1);
		connectInputs(gate, inputStr);
		connectOutputs(gate, outputStr);

		addMulGate((GateMul2*)gate);	
	} else if(strcmp(type, "zerop") == 0) {
		gate = new GateZerop(this->field);

		list<GateZerop*> _gates = static_cast<GateZerop*>(gate)->getPolyGeneratorList();
		
		istringstream iss (outputStr,istringstream::in);
		long wireId;

		for(list<GateZerop*>::iterator it = _gates.begin(); it != _gates.end(); it++)
		{
			connectInputs(*it, inputStr);
			iss >> wireId;
			(*it)->assignOutput(wires[wireId]);
			addMulGate(*it);
		}
		
		//connectOutputs(_gates.back(), outputStr);
	/* // Still need to figure out how to add the const to the polynomials (mod.add currently not used)
	} else if (strstr(type, "const-add-")) {
		int constant = 0;
		int ret = sscanf_s(type, "const-add-%d", &constant);
		assert(ret == 1);
		gate = new GateAddConst(this->field, constant);
		addNonMulGate(gate);
		*/
	} else if (strstr(type, "const-mul-neg-")) {
		FieldElt constant;
		char* constStr = type + sizeof("const-mul-neg-") - 1;
		scan_io_value(constStr, (unsigned char*) constant, sizeof(FieldElt));

		gate = new GateMulConst(this->field, constant, true);

		// Connect all of the wires
		assert(numGateInputs == 1);
		assert(numGateOutputs == 1);
		connectInputs(gate, inputStr);
		connectOutputs(gate, outputStr);

		addNonMulGate(gate);
	} else if (strstr(type, "const-mul-")) {
		FieldElt constant;
		char* constStr = type + sizeof("const-mul-") - 1;
		scan_io_value(constStr, (unsigned char*) constant, sizeof(FieldElt));

		gate = new GateMulConst(this->field, constant);

		// Connect all of the wires
		assert(numGateInputs == 1);
		assert(numGateOutputs == 1);
		connectInputs(gate, inputStr);
		connectOutputs(gate, outputStr);

		addNonMulGate(gate);
	} else if (strstr(type, "sel-bit-")) {
		int constant = 0;
		int ret = sscanf_s(type, "sel-bit-%d", &constant);
		assert(ret == 1);

		assert(0);	// Not yet implemented
#if 0
		// Create the "parent"
		GateBitSel* parent = new GateBitSel(this->field, wires[outputWireId]);
		addMulGate(parent);

		// Create the child
		GateBitSel* child = new GateBitSel(this->field, constant, wires[outputWireId], parent);
		addMulGate(child);

		// Connect the input wire to both parent and child
		istringstream iss (inputStr,istringstream::in);
		long wireId;	
		while (iss >> wireId) {
			parent->assignInput(wires[wireId]);
			child->assignInput(wires[wireId]);
		}
#endif
	
	} else if (strstr(type, "split")) {
		// Create the "parent"
		GateBitSplitMany* parent = new GateBitSplitMany(this->field, numGateOutputs, numGateOutputs, NULL);

		// Add the input (no output for the parent)
		assert(numGateInputs == 1);
		connectInputs(parent, inputStr);

		addMulGate((GateMul2*)parent);

		// Now create the children
		long wireId, index;	
		index = 0;
		istringstream iss (outputStr,istringstream::in);
		while (iss >> wireId) {
			gate = new GateBitSplitMany(this->field, numGateOutputs, index++, parent);
			connectInputs(gate, inputStr);
			gate->assignOutput(wires[wireId]);	 

			addMulGate((GateMul2*)gate);
		}		
	} else {
		assert(0);	// Unexpected gate type
	}
}


void Circuit::parse(const string filename) {
	long wireId, numWires, numGateInputs, numGateOutputs;
	wireId = numWires = numGateInputs = numGateOutputs = 0;
	char type[200];
	char* inputStr;
	char* outputStr;

	ifstream ifs ( filename , ifstream::in );
	if(!ifs.good()) {
		printf("Unable to open circuit file: %s\n", filename.c_str());
		exit(5);
	}
	string line;
	getline(ifs, line);
	int ret = sscanf_s(line.c_str(), "total %d", &numWires);
	assert(ret == 1);
		
	freshWires(this->wires, numWires);

	while (getline(ifs, line)) {
		if (line.length() == 0) {	// Skip it
			continue;
		}
		// Assume the worst
		inputStr = new char[line.size()];
		outputStr = new char[line.size()];

		if (line[0] == '#') { // Ignore comments
			continue;
		} else if (1 == sscanf_s(line.c_str(), "input %d", &wireId)) {
			// Explicitly mark this wire as an input
			wires[wireId]->trueInput = true;
		} else if (1 == sscanf_s(line.c_str(), "nizkinput %d", &wireId)) {
			// Explicitly mark this wire as both a regular input and a NIZK input (i.e., part of the prover's secret witness)
			wires[wireId]->trueInput = true;
			wires[wireId]->witnessInput = true;
		} else if (1 == sscanf_s(line.c_str(), "output %d", &wireId)) {
			// Explicitly mark this wire as an output
			wires[wireId]->trueOutput = true;
		} else if (5  == sscanf_s(line.c_str(), "%s in %d <%[^>]> out %d <%[^>]>", type, sizeof(type), &numGateInputs, inputStr, line.size(), &numGateOutputs, outputStr, line.size())) {
			parseGate(type, numGateInputs, inputStr, numGateOutputs, outputStr);
		} else {
			printf("Error: In file %s, unrecognized line: %s\n", filename.c_str(), line.c_str());
			assert(0);
		}

		delete inputStr;
		delete outputStr;
	}


	ifs.close();

	// Divy the wires up into the appropriate containers
	for (int i = 0; i < wires.size(); i++) {
		if (wires[i]->trueInput) {						// Circuit input
			inputs.push_back(wires[i]);

			if (wires[i]->witnessInput) {		// Keep track of prover's secret witness input
				witness.push_back(wires[i]);
			}
		} else if (wires[i]->trueOutput) {		// Circuit output
			outputs.push_back(wires[i]);
			assert(wires[i]->input->isMult());	// All output wires should emerge from mult gates
		} else if (wires[i]->input->isMult()) {	// Mult gate in the middle of the circuit
			intermediates.push_back(wires[i]);
		} else {
			nonMultIntermediates.push_back(wires[i]);
		}
	}


}
