#pragma once

#include "BoolCircuit.h"

class CircuitBinaryAdder : public BoolCircuit {
public:
	// Generates a standard circuit for adding two n-bit numbers
	CircuitBinaryAdder(const int bitLength);

private:
	int bitLength;

	void consolidate();		// Create some helpful data structures to facilitate later operations
};