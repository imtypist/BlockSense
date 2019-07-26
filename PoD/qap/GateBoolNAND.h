#pragma once

#include "BoolGate.h"
#include "BoolWire.h"

// Creates an fan-in-N 1-element-output NAND
class GateBoolNAND : public BoolGate {
public:
	GateBoolNAND(int N);	
	~GateBoolNAND();

	inline void eval();
	inline bool op (bool a, bool b) { return !(a && b); }

	void generateSP(QSP* qsp);

	int N;
};