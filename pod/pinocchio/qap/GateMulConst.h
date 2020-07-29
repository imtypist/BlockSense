#pragma once

#include "Gate.h"
#include "Wire.h"
#include "Field.h"

// Adds a const to its input
class GateMulConst : public Gate {
public:
	GateMulConst(Field* field, FieldElt& N, bool negative = false);	
	GateMulConst(Field* field, int N);	
	~GateMulConst();

	inline void eval();
	bool isMult() { return false; }
	void updateModifier(Modifier* mod);

	virtual CachedPolynomial* getCachedPolynomial();
	virtual void cachePolynomial(CachedPolynomial* poly);
	virtual void deleteCachedPolynomial();

	FieldElt constant;

private:
	CachedPolynomial* cachedPoly;
};