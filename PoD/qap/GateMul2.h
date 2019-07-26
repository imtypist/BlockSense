#pragma once

#include "Gate.h"
#include "Wire.h"
#include "QAP.h"
#include "SparsePolynomial.h"
#include <forward_list>

class QAP;
class GateMul2;
class Circuit;

// Used when we construct the QAP.  Tracks the effects of const mul/add gates.
typedef struct {
	Wire* wire;
	Modifier mod;
	CachedPolynomial* cachedPoly;
} WireMod;
typedef std::forward_list<WireMod*> WireModList;

class GateMul2 : public Gate {
public:
	GateMul2();
	GateMul2(Field* field, Wire* in1 = NULL, Wire* in2 = NULL, Wire* out = NULL);
	virtual ~GateMul2();

	virtual void eval();
	bool isMult() { return true; }
	virtual void updateModifier(Modifier* mod) { ; }	// No change
	virtual void generatePolys(QAP* qap, bool cachedBuild);
	virtual void assignPolyCoeff(QAP* qap);
	virtual int polyCount();		// How many polys does this gate need.  Most need 0 or 1.
	virtual CachedPolynomial* getCachedPolynomial() { return NULL; }	// Muls don't need to cache
	virtual void cachePolynomial(CachedPolynomial* poly) { ; } // Muls don't need to cache
	virtual void deleteCachedPolynomial() { ; }  // Nothing to delete

	unsigned long rootId;
	unsigned long polyId;

protected:
	void recurseTopological(QAP* qap, Wire* wire, SparsePolynomial* polys, Modifier mod, bool cachedBuild);
};