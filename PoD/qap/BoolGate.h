#pragma once

#include "Types.h"
#include "BoolWire.h"

class QSP;
class BoolWire;
class BoolGate;

class BoolGate {
public:
	long long id;

	virtual ~BoolGate() = 0;

	virtual void eval() = 0;
	virtual void generateSP(QSP* qsp) = 0;
	// Given two bits as input, what does this gate return?
	virtual bool op (bool a, bool b) = 0;

	//virtual void getPolys() = 0;

	virtual void assignInput(BoolWire* wire);
	void assignOutput(BoolWire* wire);

	virtual bool testAndSetReady();

	BoolWire** inputs;
	BoolWire* output;

	SparsePolyPtrVector polys;	// Polynomials representing this gate's span program

	int numInputs() { return assignedInputs; }
	void genericSP(QSP* qsp);

protected:

	SparsePolyPtrVector* getOutput(bool a, bool b);

	int maxInputs;
	int assignedInputs;
};