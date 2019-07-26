#pragma once

#include "Circuit.h"

class CircuitMatrixMultiply : public Circuit {
public:
	// Generates a standard circuit for matrix multiplication
	CircuitMatrixMultiply(Field* field, const int dimension);

	// Generates a circuit suitable for QAP creation
	// fanIn2adders governs whether we divide fan-in-N adders into fanIn2adders
	CircuitMatrixMultiply(Field* field, const int dimension, bool fanIn2adders);

private:
	int dimension;
	bool fanIn2adders;

	void directEval();
	void labelWires();
};