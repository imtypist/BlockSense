#pragma once

#include "GateMul2.h"
#include "Wire.h"

class GateBitSplitMany : public GateMul2 {
public:
	GateBitSplitMany();
	// When we create a "split gate", we actually create many instances
	// of this class, one to represent each outbound bit
	// N = total number of bits to split, index indictes which bit this gate represents
	// If index = N, then the output is the original input value
	GateBitSplitMany(Field* field, int N, int index, GateBitSplitMany* parent);
	
	virtual inline void eval();

	virtual void generatePolys(QAP* qap, bool cachedBuild);
	virtual void assignPolyCoeff(QAP* qap);

	virtual bool testAndSetReady();

	virtual int polyCount();		// How many polys does this gate need.  Most need 0 or 1.

protected:
	int N;
	int index;	
	GateBitSplitMany* parent;

	virtual inline bool amParent();	// Am I the parent?
};