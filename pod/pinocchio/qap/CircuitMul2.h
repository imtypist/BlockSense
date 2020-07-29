#pragma once

// Simple little circuit: Contains exactly 1 mul2 gate

#include "Circuit.h"

class CircuitMul2 : public Circuit {
public:
	// Generates a standard circuit for matrix multiplication
	CircuitMul2(Field* field);

private:
	void directEval();
};