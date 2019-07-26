#include <assert.h>
#include "GateBitSplitMany.h"
#include "Field.h"

GateBitSplitMany::GateBitSplitMany() {
	this->N = -1;
	this->index = -2;
	this->inputs = NULL;
	this->parent = NULL;
	this->output = NULL;
}

// When we create a "split gate", we actually create many instances
// of this class, one to represent each outbound bit
// N = total number of bits to split, index indictes which bit this gate represents
// If index = N, then the output is the original input value
GateBitSplitMany::GateBitSplitMany(Field* field, int N, int index, GateBitSplitMany* parent)  {
	this->field = field;
	this->N = N;
	this->index = index;
	this->inputs = new Wire*[1];
	this->parent = parent;

	maxInputs = 1;
	assignedInputs = 0;
	
	inputs[0] = NULL;
	
	output = NULL;	
}

bool GateBitSplitMany::amParent() {
	return this->parent == NULL;
}

inline void GateBitSplitMany::eval() {
	assert(inputs);

	if (amParent()) {
		// We're the parent with no output, so just return
		return;
	}

	assert(output);
	field->zero(output->value);
	
	// Grab the index-th most-signficant bit from the input
	int bit = field->bit(index, inputs[0]->value);

	if (bit == 0) {
		field->zero(output->value);
	} else {
		assert(bit == 1);
		field->one(output->value);
	}	
}

bool GateBitSplitMany::testAndSetReady() {
	assert(inputs);

	// Check that our input is ready
	if (!inputs[0]->visited) {
		return false;
	}

	if (!amParent()) {		// We're not the parent, so set our output to ready (parent has no output)
		output->visited = true;
	}

	return true;
}

// Assign the value of this gate's output as the coefficient for all of the polynomials
void GateBitSplitMany::assignPolyCoeff(QAP* qap) {
	if (!amParent()) {	// Parent doesn't have a poly
		qap->V[qap->polyIndex(this)].coeff = &this->output->value;
		qap->W[qap->polyIndex(this)].coeff = &this->output->value;
		qap->Y[qap->polyIndex(this)].coeff = &this->output->value;
	}
}

// How many polys does this gate need.  Most need 0 or 1.
int GateBitSplitMany::polyCount() {
	// Everyone except the parent needs 1 poly
	if (amParent()) {
		return 0;
	} else {
		return 1;
	}
}


void GateBitSplitMany::generatePolys(QAP* qap, bool cachedBuild) {
	if (amParent()) {
		// For N, v_N(r) = 0, so no work needed

		// All of the w's are 0, except w_0(r) = 1
		FieldElt one;
		field->one(one);
		qap->W[0].addNonZeroRoot(this->rootId, one, field);

		// All of the y's are 0, except y_me(r) = 1, which we assign recursively to all of the gate's parents
		Modifier mod;	// Create a NOP modifier to start with
		field->zero(mod.add);
		field->one(mod.mul);

		recurseTopological(qap, inputs[0], qap->Y, mod, cachedBuild);

		return;
	}

	// Otherwise, we're a child

	////////////// First condition //////////////////
	// Checks that we chose the bit from the right spot
	// Uses one root, corresponding to the parent's (index N's) root
	// This gives us p(r) = v(r)w(r)-y(r) = (\sum a_i * 2^{i-1}) * 1 = a

	// Handle the V's.  
	// For all but N, v_i(r) = 2^{i-1} = 2^{index} b/c index is 0-based

	FieldElt two, val;
	field->set(two, 2);
	field->exp(two, index, val);
		
	qap->V[qap->polyIndex(this)].addNonZeroRoot(parent->rootId, val, field);

	// w_i(r) = 0

	// y_i(r) = 0, except for y_1(r) = 1, which is handled by the parent above


	////////////// Second condition //////////////////
	// Checks that the outputs are all bits (0 or 1)
	// Uses one root per bit
	// This gives us p(r_i) = v(r_i)w(r_i)-y(r_i) = a_i * (1 - a_i) = 0

	// Handle the V's.  
	// v_i(r_i) = 1
	FieldElt one;
	field->one(one);
	qap->V[qap->polyIndex(this)].addNonZeroRoot(this->rootId, one, field);

	// w_0(r_i) = 1, w_i(r_i) = -1
	qap->W[0].addNonZeroRoot(this->rootId, one, field);

	FieldElt zero, negOne;
	field->zero(zero);
	field->sub(zero, one, negOne);
	qap->W[qap->polyIndex(this)].addNonZeroRoot(this->rootId, negOne, field);

	// y_i(r_i) = 0, so no work needed

}