#include "BoolCircuit.h"
#include "GateBoolAND.h"
#include "GateBoolNAND.h"
#include "GateBoolOR.h"
#include "GateBoolXOR.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <assert.h>
using namespace std;

BoolCircuit::BoolCircuit(const string filename) {
	parse(filename);
	consolidate();
	sort();
	assignIDs();
}

BoolCircuit::~BoolCircuit() {

	// Delete all of the gates
	while(!gates.empty()) {
		delete gates.front();
		gates.pop_front();
	}

	// Delete all of the wires
	for (BoolWireVector::iterator iter = wires.begin();
		 iter != wires.end();
		 iter++) {
		delete (*iter);
	}
}


void BoolCircuit::eval() {
	for (BoolGateList::iterator iter = gates.begin();
		   iter != gates.end();
			 iter++) {
		(*iter)->eval();
	}
}

// Topologically sort the gates
// TODO: Technically, addGate could be maintained as a heap to avoid this step
void BoolCircuit::sort() {
	for (int i = 0; i < inputs.size(); i++) {
		inputs[i]->visited = true;
	}

	size_t numUnReady = gates.size();

	while (numUnReady > 0) {
		// Iterate through the unready circuits at the front of the list
		int numReady = 0;
		BoolGateList::iterator iter = gates.begin();
		for (int i = 0; i < numUnReady; i++) {
			BoolGate* g = *iter; 
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


void BoolCircuit::addGate(BoolGate* gate) {
	gates.push_back(gate);
}

BoolWire* BoolCircuit::freshBoolWire(BoolWireVector& vector) {
	int index = freshBoolWires(vector, 1);
	return vector[index];
}

// Generate num new wires and add them to vector.  Returns index of first new wire.
int BoolCircuit::freshBoolWires(BoolWireVector& vector, int num) {
	int oldSize = (int) vector.size();
	vector.resize(oldSize + num);

	for (int i = 0; i < num; i++) {
		vector[oldSize + i] = new BoolWire();
	}

	return oldSize;
}

// It is convenient to assign unique IDs to each of gates
void BoolCircuit::assignIDs() {
	for (int i = 0; i < inputs.size(); i++) {
		inputs[i]->id = i;
	}

	long long id = 0;
	for (BoolGateList::iterator iter = gates.begin();
		iter != gates.end();
		iter++) {
		(*iter)->id = id++;
	}
}

BoolGate* BoolCircuit::parseGate(char* type, int numGateInputs, char* inputStr, int numGateOutputs, char* outputStr) {
	BoolGate* gate = NULL;

	// We only handle 2-to-1 bool gates at the moment
	assert(numGateInputs == 2);
	assert(numGateOutputs == 1);
		
	// Deduce the type
	if (strcmp(type, "and") == 0) {
		gate = new GateBoolAND(numGateInputs);
	} else if (strcmp(type, "or") == 0) {
		gate = new GateBoolOR(numGateInputs);
	} else if (strcmp(type, "xor") == 0) {
		gate = new GateBoolXOR(numGateInputs);
	} else if (strcmp(type, "nand") == 0) {
		gate = new GateBoolNAND(numGateInputs);
	} else {
		assert(0);	// Unexpected gate type
	}

	// Connect all of the input wires
	long wireId;	
	istringstream iss (inputStr,istringstream::in);
	while (iss >> wireId) {
		gate->assignInput(wires[wireId]);
	}

	// Connect the output wire
	istringstream oss (outputStr,istringstream::in);
	oss >> wireId;
	gate->assignOutput(wires[wireId]);

	// Add this gate to the list of known gates in the circuit
	addGate(gate);

	return gate;
}

void BoolCircuit::parse(const string filename) {
	long wireId, numWires, numGateInputs, numGateOutputs;
	wireId = numWires = numGateInputs = numGateOutputs = 0;
	char type[200], inputStr[2000], outputStr[2000];

	ifstream ifs ( filename , ifstream::in );
	if(!ifs.good()) {
		printf("Unable to open circuit file: %s\n", filename.c_str());
		exit(5);
	}
	string line;
	getline(ifs, line);
	int ret = sscanf_s(line.c_str(), "total %d", &numWires);
	assert(ret == 1);

	freshBoolWires(this->wires, numWires);
	
	while (getline(ifs, line)) {
		if (line[0] == '#') { // Ignore comments
			continue;
		} else if (1 == sscanf_s(line.c_str(), "input %d", &wireId)) {
			// Skip past inputs
		} else if (1 == sscanf_s(line.c_str(), "output %d", &wireId)) {
			// Skip past outputs
		} else if (5  == sscanf_s(line.c_str(), "%s in %d <%[^>]> out %d <%[^>]>", type, sizeof(type), &numGateInputs, inputStr, sizeof(inputStr), &numGateOutputs, outputStr, sizeof(outputStr))) {	
			parseGate(type, numGateInputs, inputStr, numGateOutputs, outputStr);
		} else {
			printf("Error: Unrecognized line: %s\n", filename.c_str());
			assert(0);
		}
	}
	ifs.close();
}


// Create some helpful data structures to facilitate later operations
void BoolCircuit::consolidate() {
	for (BoolWireVector::iterator iter = wires.begin();
	     iter != wires.end();
		 iter++) {
		if ((*iter)->input == NULL) {		// Circuit input
			inputs.push_back(*iter);
			ioWires.push_back(*iter);
		} else if ((*iter)->output == NULL) {		// Circuit output
			outputs.push_back(*iter);
			ioWires.push_back(*iter);
		} else {	// Stuck in the middle with you
			intermediates.push_back(*iter);
		}
	}
}