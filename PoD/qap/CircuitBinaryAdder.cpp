#include <assert.h>
#include "CircuitBinaryAdder.h"

#include "GateBoolAND.h"
#include "GateBoolNAND.h"
#include "GateBoolOR.h"
#include "GateBoolXOR.h"

// Generates a basic ripple-carry circuit for addition
// out_i = (A_i xor B_i) xor carry_{i-1}
// carry_i =  ((A_i xor B_i) & carry_{i-1}) || (A_i & B_i) 
CircuitBinaryAdder::CircuitBinaryAdder(const int bitLength) {
	this->bitLength = bitLength;

	// Allocate I/O
	freshBoolWires(inputs, 2*bitLength); // 2 numbers
	freshBoolWires(outputs, bitLength+1); // 1 number + 1 overflow bit
	
	// Create the addition layer
	for (int i = 0; i < bitLength; i++) {
		int indexA = 0 * bitLength + i;	// i-th bit of the first number
		int indexB = 1 * bitLength + i;	// i-th bit of the second number
		
		// Initially, there's no carry in, so life's simple
		if (i == 0) {
			// A xor B
			BoolGate* gate = new GateBoolXOR(2);
			gate->assignInput(inputs[indexA]);
			gate->assignInput(inputs[indexB]);
			gate->assignOutput(outputs[i]);
			addGate(gate);

			// Carry_out = A & B
			gate = new GateBoolAND(2);
			gate->assignInput(inputs[indexA]);
			gate->assignInput(inputs[indexB]);
			gate->assignOutput(freshBoolWire(intermediates));
			addGate(gate);
		} else {
			const int base = 1;	// Account for the 1 we used in the i == 0 case
			int indexOldCarry = base + 4*(i-1) - 1;
			int indexAxorB = base + 4*(i-1);
			int indexAxorBandCarry = base + 4*(i-1) + 1;
			int indexAandB = base + 4*(i-1)+2;
			//int indexNewCarry = 4*(i-1) + 3;

			// A xor B
			BoolGate* gate = new GateBoolXOR(2);
			gate->assignInput(inputs[indexA]);
			gate->assignInput(inputs[indexB]);
			gate->assignOutput(freshBoolWire(intermediates));
			addGate(gate);

			// A xor B xor C_{i-1}
			gate = new GateBoolXOR(2);
			gate->assignInput(intermediates[indexAxorB]);
			gate->assignInput(intermediates[indexOldCarry]);
			gate->assignOutput(outputs[i]);
			addGate(gate);

			// (A xor B) && C_{i-1}
			gate = new GateBoolAND(2);
			gate->assignInput(intermediates[indexAxorB]);
			gate->assignInput(intermediates[indexOldCarry]);
			gate->assignOutput(freshBoolWire(intermediates));
			addGate(gate);

			// A & B
			gate = new GateBoolAND(2);
			gate->assignInput(inputs[indexA]);
			gate->assignInput(inputs[indexB]);
			gate->assignOutput(freshBoolWire(intermediates));
			addGate(gate);

			if (i != bitLength - 1) { 
				// carry_i =  ((A_i xor B_i) & carry_{i-1}) || (A_i & B_i) 
				gate = new GateBoolOR(2);
				gate->assignInput(intermediates[indexAxorBandCarry]);
				gate->assignInput(intermediates[indexAandB]);
				gate->assignOutput(freshBoolWire(intermediates));
				addGate(gate);
			} else { // Final carry bit is an ouput
				// out_{N+1} =  ((A_i xor B_i) & carry_{i-1}) || (A_i & B_i) 
				gate = new GateBoolOR(2);
				gate->assignInput(intermediates[indexAxorBandCarry]);
				gate->assignInput(intermediates[indexAandB]);
				gate->assignOutput(outputs[bitLength]);
				addGate(gate);
			}
		}
	}

	this->sort();
	this->assignIDs();
	this->consolidate();	
}

// Create some helpful data structures to facilitate later operations
void CircuitBinaryAdder::consolidate() {
        ioWires.reserve(inputs.size() + outputs.size());
        wires.reserve(inputs.size() + outputs.size() + intermediates.size());

        for (BoolWireVector::iterator iter = inputs.begin();
             iter != inputs.end();
                 iter++) {
                ioWires.push_back(*iter);
                wires.push_back(*iter);
        }

        for (BoolWireVector::iterator iter = intermediates.begin();
             iter != intermediates.end();
                 iter++) {
                wires.push_back(*iter);
        }

        for (BoolWireVector::iterator iter = outputs.begin();
             iter != outputs.end();
                 iter++) {
                ioWires.push_back(*iter);
                wires.push_back(*iter);
        }
}


// Just takes the NAND of each pair of bits
/*
CircuitBinaryAdder::CircuitBinaryAdder(const int bitLength) {
	this->bitLength = bitLength;

	// Preallocate space (resize for static allocation, reserve for allocation via freshWire())
	inputs.resize(2*bitLength); // 2 numbers
	assert(inputs.size() == 2*bitLength);
	outputs.resize(bitLength); // 1 number
	
	// Create the addition layer
	for (int i = 0; i < bitLength; i++) {
		int indexA = 0 * bitLength + i;	// i-th bit of the first number
		int indexB = 1 * bitLength + i;	// i-th bit of the second number

		//GateBoolAND* and = new GateBoolAND(2);	// TODO: Should be AND
		GateBoolNAND* nand = new GateBoolNAND(2);
		nand->assignInput(&inputs[indexA]);
		nand->assignInput(&inputs[indexB]);
		nand->assignOutput(&outputs[i]);
		addGate(nand);
	}

	this->assignIDs();
	this->consolidate();
}
*/