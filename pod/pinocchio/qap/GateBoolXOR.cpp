#include <assert.h>
#include "GateBoolXOR.h"

GateBoolXOR::GateBoolXOR(int N) {
	this->N = N;
	inputs = new BoolWire*[N];
	maxInputs = N;
	assignedInputs = 0;

	output = NULL;	
}

GateBoolXOR::~GateBoolXOR() {
	delete [] inputs;
}

inline void GateBoolXOR::eval() {
	assert(inputs && output);

	bool result = inputs[0]->value;
	for (int i = 1; i < N; i++) {
		result = result ^ inputs[i]->value;
	}
	output->value = result;
}


void GateBoolXOR::generateSP(QSP* qsp) {
	this->genericSP(qsp);
}