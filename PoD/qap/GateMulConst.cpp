#include <assert.h>
#include "GateMulConst.h"

GateMulConst::GateMulConst(Field* field, int N) : Gate(field) {
	inputs = new Wire*[1];
	maxInputs = 1;
	assignedInputs = 0;

	field->set(constant, N);

	output = NULL;	
	cachedPoly = NULL;
}

GateMulConst::GateMulConst(Field* field, FieldElt& N, bool negative) : Gate(field) {
	inputs = new Wire*[1];
	maxInputs = 1;
	assignedInputs = 0;

	if (negative) {
		FieldElt zero;
		field->zero(zero);
		field->sub(zero, N, constant);
	} else {
		field->copy(N, constant);
	}

	output = NULL;	
	cachedPoly = NULL;
}


GateMulConst::~GateMulConst() {
	delete [] inputs;
}

inline void GateMulConst::eval() {
	assert(inputs && output);

	field->mul(inputs[0]->value, constant, output->value);
}

void GateMulConst::updateModifier(Modifier* mod) {
	field->mul(mod->mul, constant, mod->mul);
}


CachedPolynomial* GateMulConst::getCachedPolynomial() {
	return cachedPoly;
}

void GateMulConst::cachePolynomial(CachedPolynomial* poly) {
	assert(cachedPoly == NULL);
	cachedPoly = poly;
}

void GateMulConst::deleteCachedPolynomial() {
	delete cachedPoly;
}