#pragma once

#include "Gate.h"
#include "Wire.h"

// Creates an fan-in-N 1-element-output adder
class GateAddN : public Gate {
public:
	GateAddN(Field* field, int N);	
	~GateAddN();

	inline void eval();
	bool isMult() { return false; }
	void updateModifier(Modifier* mod) { ; }	// No change

	virtual CachedPolynomial* getCachedPolynomial();
	virtual void cachePolynomial(CachedPolynomial* poly);
	virtual void deleteCachedPolynomial();

private:
	int N;
	CachedPolynomial* cachedPoly;
};