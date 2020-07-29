#pragma once

#include "BoolGate.h"
#include "BoolWire.h"

// Creates an fan-in-N 1-element-output OR
class GateBoolOR : public BoolGate {
public:
	GateBoolOR(int N);	
	~GateBoolOR();

	inline void eval();
	inline bool op (bool a, bool b) { return a || b; }
	void generateSP(QSP* qsp);

	int N;
};