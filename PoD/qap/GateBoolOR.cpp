#include <assert.h>
#include "GateBoolOR.h"

GateBoolOR::GateBoolOR(int N) {
	this->N = N;
	inputs = new BoolWire*[N];
	maxInputs = N;
	assignedInputs = 0;

	output = NULL;	
}

GateBoolOR::~GateBoolOR() {
	delete [] inputs;
}

inline void GateBoolOR::eval() {
	assert(inputs && output);

	bool result = false;
	for (int i = 0; i < N; i++) {
		result = result || inputs[i]->value;

	}
	output->value = result;
}


void GateBoolOR::generateSP(QSP* qsp) {
	this->genericSP(qsp);
}