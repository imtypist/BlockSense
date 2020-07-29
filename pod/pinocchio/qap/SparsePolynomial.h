#pragma once

//#include <map>
#include <hash_map>	
#include "Field.h"

using namespace stdext;

// We represent polynomials by the values they take
// on when evaluated at the target polynomial's roots,
// omitting those (many) roots where the polynomials are zero

typedef struct {
	FieldElt  value;	// The value it takes on at that root 
} NonZeroRootVal;

//typedef map<unsigned long,NonZeroRootVal*> NonZeroRoots;
typedef hash_map<unsigned long,NonZeroRootVal*> NonZeroRoots;
typedef pair<unsigned long,NonZeroRootVal*> NonZeroRoot;

class SparsePolynomial {
public:
	SparsePolynomial();
	~SparsePolynomial();

	void addNonZeroRoot(unsigned long rootID, const FieldElt& val, Field* field);

	FieldElt* coeff;	// Coefficient for this poly in a given evaluation		

	// TODO: Create a proper STL iterator
	// Warning: Definitely not thread safe!
	void iterStart();
	void iterNext();
	bool iterEnd();
	unsigned long curRootID();
	FieldElt* curRootVal();

	void addPoly(SparsePolynomial& poly, Field* field);
	void addModifiedPoly(SparsePolynomial& poly, FieldElt& mod, Field* field);

	bool equal(Field* field, SparsePolynomial &other);

	unsigned int size();

	unsigned long id;

	static unsigned long totalNzrs;

	void print(Field* field);

//private:
	NonZeroRoots nonZeroRoots;			
	NonZeroRoots::iterator curIter;
};