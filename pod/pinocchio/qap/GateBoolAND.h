#pragma once

#include "BoolGate.h"
#include "BoolWire.h"

// Creates an fan-in-N 1-element-output AND
class GateBoolAND : public BoolGate {
public:
	GateBoolAND(int N);	
	~GateBoolAND();

	inline void eval();
	inline bool op (bool a, bool b) { return a && b; }
	void generateSP(QSP* qsp);

	int N;
};