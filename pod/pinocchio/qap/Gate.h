#pragma once

#include "Types.h"
#include "Wire.h"
#include "Field.h"
#include "SparsePolynomial.h"

class Wire;
class Gate;

typedef struct sCachedPolynomial {
	SparsePolynomial me;
	sCachedPolynomial* parent;
	int completionCtr;		// Done with this poly's subtree when ctr hits 0
	Modifier initMod;			// Modifier value when this cached poly was created
	Modifier myMod;				// Modifier effect this poly's gate has
} CachedPolynomial;

class Gate {
public:
	//long long id;

	Gate();
	Gate(Field* f) : field(f) {;}
	virtual ~Gate();

	virtual void eval() = 0;

	virtual void assignInput(Wire* wire);
	void assignOutput(Wire* wire);

	Wire** inputs;
	Wire* output;

	virtual bool testAndSetReady();

	virtual bool isMult() = 0;
	virtual void updateModifier(Modifier* mod) = 0;

	virtual CachedPolynomial* getCachedPolynomial() = 0;
	virtual void cachePolynomial(CachedPolynomial* poly) = 0;
	virtual void deleteCachedPolynomial() = 0;

	int numInputs() { return assignedInputs; }

protected:
	Field* field;
	int maxInputs;
	int assignedInputs;
};