#include <assert.h>
#include "GateAddN.h"
#include "Field.h"


GateAddN::GateAddN(Field* field, int N) : Gate(field) {
	this->N = N;
	inputs = new Wire*[N];
	maxInputs = N;
	assignedInputs = 0;

	output = NULL;	
	cachedPoly = NULL;
}

GateAddN::~GateAddN() {
	delete [] inputs;
}

// TODO: We lose the opportunity to do fieldAddMany,
//       since the values are indirected through the Wires
inline void GateAddN::eval() {
	assert(inputs && output);

	field->zero(output->value);
	for (int i = 0; i < N; i++) {
		field->add(inputs[i]->value, output->value, output->value);
	}
}


CachedPolynomial* GateAddN::getCachedPolynomial() {
	return cachedPoly;
}

void GateAddN::cachePolynomial(CachedPolynomial* poly) {
	assert(cachedPoly == NULL);
	cachedPoly = poly;
}

void GateAddN::deleteCachedPolynomial() {
	delete cachedPoly;
}