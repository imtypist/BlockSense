#include "GateMul2.h"
#include "CircuitMatrixMultiply.h"
#include "Field.h"
#include "GateAddN.h"



// Generates a standard circuit for matrix multiplication
// c_{ij} = \sum_{k=1..N} a_ik * b_kj
CircuitMatrixMultiply::CircuitMatrixMultiply(Field* field, const int dimension) : Circuit(field) {
	this->dimension = dimension;
	this->fanIn2adders = false;

	// Allocate I/O
	freshWires(inputs, 2*dimension*dimension);  // 2 NxN matrices
	freshWires(outputs, dimension*dimension); // 1 NxN matrix	
	
	// Create the multiplication layer
	for (int i = 0; i < dimension; i++) {
		for (int j = 0; j < dimension; j++) {
			GateAddN* adder = new GateAddN(field, dimension);
			adder->assignOutput(freshWire(outputs));	// outputs[i][j]
			addNonMulGate(adder);
			for (int k = 0; k < dimension; k++) {
				int indexA = 0 * (dimension*dimension) + i * dimension + k; // inputs[0][i][k]
				int indexB = 1 * (dimension*dimension) + k * dimension + j; // inputs[1][k][j]

				GateMul2* mul = new GateMul2(field, inputs[indexA], inputs[indexB], freshWire(intermediates));
				addMulGate(mul);
				adder->assignInput(mul->output);				
			}
		}
	}

	this->labelWires();
	this->sort();	// Topologically sort all of the gates we just added
	this->assignIDs();
}

// Generates a circuit suitable for QAP creation
// fanIn2adders governs whether we divide fan-in-N adders into fanIn2adders
CircuitMatrixMultiply::CircuitMatrixMultiply(Field* field, const int dimension, bool fanIn2adders) : Circuit(field) {
	this->dimension = dimension;
	this->fanIn2adders = fanIn2adders;

	// Allocate I/O
	freshWires(inputs, 2*dimension*dimension+1); // 2 NxN matrices + a constant 1
	freshWires(outputs, dimension*dimension); // 1 NxN matrix	

	int constOneInputIndex = 2*dimension*dimension;
	for (int i = 0; i < dimension; i++) {
		for (int j = 0; j < dimension; j++) {
			WireList outputList;

			// Create the a_ik * b_kj gates, for k = 1 to dimension
			for (int k = 0; k < dimension; k++) {
				int indexA = 0 * (dimension*dimension) + i * dimension + k; // inputs[0][i][k]
				int indexB = 1 * (dimension*dimension) + k * dimension + j; // inputs[1][k][j]				

				GateMul2* mul = new GateMul2(field, inputs[indexA], inputs[indexB], freshWire(intermediates));
				addMulGate(mul);
				outputList.push_back(mul->output);			
			}

			// Sum the results of the multiplication gates
			if (!fanIn2adders) { 
				// Create a single fan-in-N adder
				GateAddN* adder = new GateAddN(field, dimension);
				addNonMulGate(adder);
				adder->assignOutput(freshWire(nonMultIntermediates));

				// Assign the outputs of the multiplications as inputs
				for (WireList::iterator iter = outputList.begin(); iter != outputList.end(); iter++) {
					adder->assignInput(*iter);
				}
				outputList.clear();

				// Connect a single multiplication gate to the adder's output, so the overall output comes from a mult gate
				int indexOut = i*dimension + j;
				GateMul2* mul = new GateMul2(field, adder->output, inputs[constOneInputIndex], outputs[indexOut]);
				addMulGate(mul);						
			} else {
				// Create a binary tree of fan-in-2 adders (and multiplication gates) to sum the outputs of the multiplications
				while (outputList.size() > 1) {
					Wire* out1 = outputList.front();  outputList.pop_front();					
					Wire* out2 = outputList.front();  outputList.pop_front();

					GateAddN* add2 = new GateAddN(field, 2);
					add2->assignInput(out1);
					add2->assignInput(out2);
					add2->assignOutput(freshWire(nonMultIntermediates));
					addNonMulGate(add2);				

					GateMul2* constMul = NULL;
					if (outputList.size() == 0) {
						// This is the last mul2 gate, so it's output is a circuit output
						int outputIndex = dimension * i + j;	// outputs[i][j]
						constMul = new GateMul2(field, add2->output, inputs[constOneInputIndex], outputs[outputIndex]);
					} else {						
						constMul = new GateMul2(field, add2->output, inputs[constOneInputIndex], freshWire(intermediates));
					}
					addMulGate(constMul);

					outputList.push_back(constMul->output);
				}				
			}			
		}
	}

	this->labelWires();
	this->sort();	// Topologically sort all of the gates we just added
	this->assignIDs();
}

void CircuitMatrixMultiply::labelWires() {
	for (int i = 0; i < inputs.size(); i++) {
		inputs[i]->trueInput = true;
	}

	for (int i = 0; i < outputs.size(); i++) {
		outputs[i]->trueOutput = true;
	}
}

/* 
 * This implements standard matrix multiplication,
 * but it's modified for QAPs; i.e., it keeps track
 * of intermediate multiplication outputs and it adds
 * a "multiply by 1" ahead of each output
 * Note1: We don't actually do the multiply by 1,
 *        we just keep track of the intermediate values
 * Note2: This assumes we're using dimension-input adders, 
 *        rather than 2-input adders
 */
void CircuitMatrixMultiply::directEval() {	
	if (fanIn2adders) {
		// Not going to bother simulating this
		gateEval();
	} else {
		for (int i = 0; i < dimension; i++) {
			for (int j = 0; j < dimension; j++) {
				FieldElt accumulator;
				field->zero(accumulator);
				for (int k = 0; k < dimension; k++) {
					// a * b => c
					field->mul(inputs[i*dimension+k]->value, inputs[dimension*dimension + k*dimension+j]->value, intermediates[i*dimension*dimension + j*dimension + k]->value);
					field->add(intermediates[i*dimension*dimension + j*dimension + k]->value, accumulator, accumulator);				
				}
				field->copy(accumulator, outputs[i*dimension + j]->value);
			}
		}
	}
}