#include <assert.h>
#include "GateBoolNAND.h"

GateBoolNAND::GateBoolNAND(int N) {
	this->N = N;
	inputs = new BoolWire*[N];
	maxInputs = N;
	assignedInputs = 0;

	output = NULL;	
}

GateBoolNAND::~GateBoolNAND() {
	delete [] inputs;
}

inline void GateBoolNAND::eval() {
	assert(inputs && output);

	bool result = true;
	for (int i = 0; i < N; i++) {
		result = result && inputs[i]->value;
	}
	output->value = !result;
}

void GateBoolNAND::generateSP(QSP* qsp) {
	this->genericSP(qsp);
}