#pragma once

#include "BoolGate.h"
#include "BoolWire.h"

// Creates an fan-in-N 1-element-output OR
class GateBoolXOR : public BoolGate {
public:
	GateBoolXOR(int N);	
	~GateBoolXOR();

	inline void eval();
	inline bool op (bool a, bool b) { return (a && !b) || (!a && b); }
	void generateSP(QSP* qsp);

	int N;
};