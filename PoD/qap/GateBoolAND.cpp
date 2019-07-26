#include <assert.h>
#include "GateBoolAND.h"

GateBoolAND::GateBoolAND(int N) {
	this->N = N;
	inputs = new BoolWire*[N];
	maxInputs = N;
	assignedInputs = 0;

	output = NULL;	
}

GateBoolAND::~GateBoolAND() {
	delete [] inputs;
}

inline void GateBoolAND::eval() {
	assert(inputs && output);

	bool result = true;
	for (int i = 0; i < N; i++) {
		result = result && inputs[i]->value;

	}
	output->value = result;
}


void GateBoolAND::generateSP(QSP* qsp) {
	this->genericSP(qsp);
}