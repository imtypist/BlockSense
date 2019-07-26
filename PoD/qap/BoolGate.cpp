#include "Types.h"
#include <assert.h>
#include "BoolGate.h"
#include "QSP.h"

BoolGate::~BoolGate() {
	;
}

void BoolGate::assignOutput(BoolWire* wire) {
	assert(wire);
	assert(!output);	// Otherwise we overwrite an existing output wire!
	this->output = wire;
	wire->input = this;
}

void BoolGate::assignInput(BoolWire* wire) {
	assert(wire);
	assert(assignedInputs < maxInputs);
	inputs[assignedInputs++] = wire;
	wire->output = this;
}

// If this gate's inputs have been visited, set it's output to visited, and return true
// Else, false
bool BoolGate::testAndSetReady() {
	for (int i = 0; i < assignedInputs; i++) {
		if (!inputs[i]->visited) {
			return false;
		}
	}
	output->visited = true;
	return true;
}

SparsePolyPtrVector* BoolGate::getOutput(bool a, bool b) {
	return this->op(a, b) ? &output[0].one : &output[0].zero;
}

void BoolGate::genericSP(QSP* qsp) {
	int rootIndex = qsp->genFreshRoots(9, qsp->vTargetRoots);
	polys.reserve(9);
	SparsePolynomial* sparsePoly;

	// Create 8 independent vectors/polys, 4 for each input wire
	for (int i = 0; i < 8; i++) {
		sparsePoly = qsp->newSparsePoly();				
		polys.push_back(sparsePoly);
		FieldElt one;
		qsp->field->one(one);
		sparsePoly->addNonZeroRoot(rootIndex + i, one, qsp->field);

		if (i < 2) {
			inputs[0]->zero.push_back(sparsePoly);
		} else if (i < 4) {
			inputs[0]->one.push_back(sparsePoly);
		} else if (i < 6) {
			inputs[1]->zero.push_back(sparsePoly);
		} else {
			inputs[1]->one.push_back(sparsePoly);
		}
	}

	// Create the four output wires

	// Rest will be -1
	FieldElt zero, one, negOne;
	qsp->field->zero(zero);
	qsp->field->one(one);
	qsp->field->sub(zero, one, negOne);

	// Output wire 1
	sparsePoly = qsp->newSparsePoly();	
	polys.push_back(sparsePoly);
	getOutput(true, true)->push_back(sparsePoly);
	sparsePoly->addNonZeroRoot(rootIndex + 2, negOne, qsp->field);	
	sparsePoly->addNonZeroRoot(rootIndex + 6, negOne, qsp->field);

	// All output wires include the target's "special" 1 (target is (0, 0, ..., 0, 1))
	sparsePoly->addNonZeroRoot(rootIndex + 8, one, qsp->field);

	// Output wire 2
	sparsePoly = qsp->newSparsePoly();
	polys.push_back(sparsePoly);
	getOutput(true, false)->push_back(sparsePoly);
	sparsePoly->addNonZeroRoot(rootIndex + 3, negOne, qsp->field);
	sparsePoly->addNonZeroRoot(rootIndex + 4, negOne, qsp->field);
	sparsePoly->addNonZeroRoot(rootIndex + 8, one, qsp->field);
	
	// Output wire 3
	sparsePoly = qsp->newSparsePoly();
	polys.push_back(sparsePoly);
	getOutput(false, true)->push_back(sparsePoly);
	sparsePoly->addNonZeroRoot(rootIndex + 0, negOne, qsp->field);
	sparsePoly->addNonZeroRoot(rootIndex + 7, negOne, qsp->field);
	sparsePoly->addNonZeroRoot(rootIndex + 8, one, qsp->field);
	
	// Output wire 4
	sparsePoly = qsp->newSparsePoly();
	polys.push_back(sparsePoly);
	getOutput(false, false)->push_back(sparsePoly);
	sparsePoly->addNonZeroRoot(rootIndex + 1, negOne, qsp->field);
	sparsePoly->addNonZeroRoot(rootIndex + 5, negOne, qsp->field);
	sparsePoly->addNonZeroRoot(rootIndex + 8, one, qsp->field);

	// Finally, extend the current v_0 with the _negative_ of the target vector for this SP (target is (0, 0, ..., 0, 1))
	sparsePoly = qsp->getSPtarget();
	sparsePoly->addNonZeroRoot(rootIndex + 8, negOne, qsp->field);
	
	// Assign the appropriate polynomials to each wire
	assert(assignedInputs == 2);
	if (inputs[0]->value && inputs[1]->value) {
		inputs[0]->eval.push_back(polys[2]);
		inputs[1]->eval.push_back(polys[6]);
		output->eval.push_back(polys[8]);
	} else if (inputs[0]->value && !inputs[1]->value) {
		inputs[0]->eval.push_back(polys[3]);
		inputs[1]->eval.push_back(polys[4]);
		output->eval.push_back(polys[9]);
	} else if (!inputs[0]->value && inputs[1]->value) {
		inputs[0]->eval.push_back(polys[0]);
		inputs[1]->eval.push_back(polys[7]);
		output->eval.push_back(polys[10]);
	} else if (!inputs[0]->value && !inputs[1]->value) {
		inputs[0]->eval.push_back(polys[1]);
		inputs[1]->eval.push_back(polys[5]);
		output->eval.push_back(polys[11]);
	} else {
		assert(0);	// Shouldn't reach here
	}
}


