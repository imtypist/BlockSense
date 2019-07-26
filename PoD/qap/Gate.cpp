#include <assert.h>
#include "Gate.h"

Gate::Gate() {
	field = NULL;
	maxInputs = -5;
	assignedInputs = 0;
}

Gate::~Gate() {
	;
}

void Gate::assignOutput(Wire* wire) {
	assert(wire);
	assert(!this->output);	// Otherwise we overwrite an existing output wire!
	this->output = wire;
	wire->input = this;
}

void Gate::assignInput(Wire* wire) {
	assert(wire);
	assert(assignedInputs < maxInputs);
	inputs[assignedInputs++] = wire;
	wire->output = this;
}

// If this gate's inputs have been visited, set its output to visited, and return true
// Else, false
bool Gate::testAndSetReady() {
	for (int i = 0; i < assignedInputs; i++) {
		if (!inputs[i]->visited) {
			return false;
		}
	}
	output->visited = true;
	return true;
}