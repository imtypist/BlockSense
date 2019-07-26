#pragma once

#include "Gate.h"
#include "Wire.h"
#include "QAP.h"
#include "SparsePolynomial.h"
#include <forward_list>
#include <list>

using namespace std;

class QAP;
class GateMul2;
class Circuit;

// The equals-zero gate
class GateZerop : public GateMul2 {
public:
	GateZerop(Field * field);
	virtual ~GateZerop();
	void eval();
	bool isMult(){return true;}

	void updateModifer(Modifier*){}
#if 0
	virtual CachedPolynomial* getCachedPolynomial();
	virtual void cachePolynomial(CachedPolynomial* poly);
	virtual void deleteCachedPolynomial(CachedPolynomial* poly);
#endif
	virtual void generatePolys(QAP* qap, bool cachedBuild);
	virtual void assignPolyCoeff(QAP* qap);

	virtual int polyCount(){return 1;}

	virtual list<GateZerop*> getPolyGeneratorList();

	bool testAndSetReady();
	
	GateZerop * next, *prev;
protected:
	FieldElt zero, one, inv;

	int r2;
	
	CachedPolynomial* cachedPoly;
};