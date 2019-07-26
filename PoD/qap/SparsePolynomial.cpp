#include "SparsePolynomial.h"
#include <assert.h>

unsigned long SparsePolynomial::totalNzrs = 0;

SparsePolynomial::SparsePolynomial() {
	coeff = NULL; 
}

SparsePolynomial::~SparsePolynomial() {
	for (NonZeroRoots::iterator iter = nonZeroRoots.begin();
		   iter != nonZeroRoots.end();
			 iter++) {
		delete iter->second;
	}
	//nonZeroRoots.clear();
}

void SparsePolynomial::addNonZeroRoot(unsigned long rootID, const FieldElt& val, Field* field) {
	// Do we already have an entry for this root?
	NonZeroRoots::iterator result = nonZeroRoots.find(rootID);

	if (result != nonZeroRoots.end()) {	// We already know about this root
		field->add(val, result->second->value, result->second->value);
	} else {
		// Create a new entry
		NonZeroRootVal* nzr = new NonZeroRootVal;		SparsePolynomial::totalNzrs++;
		field->copy(val, nzr->value);

		// Add it to our collection
		nonZeroRoots.insert(NonZeroRoot(rootID, nzr));
	}
}

void SparsePolynomial::iterStart() {
	this->curIter = nonZeroRoots.begin();
}

void SparsePolynomial::iterNext() {
	this->curIter++;
}

bool SparsePolynomial::iterEnd() {
	return this->curIter == nonZeroRoots.end();
}

unsigned long SparsePolynomial::curRootID() {
	assert(!this->iterEnd());
	return curIter->first;
}

FieldElt* SparsePolynomial::curRootVal() {
	return &curIter->second->value;
}

void SparsePolynomial::addPoly(SparsePolynomial& poly, Field* field) {
	for (poly.iterStart(); !poly.iterEnd(); poly.iterNext()) {
		this->addNonZeroRoot(poly.curRootID(), *poly.curRootVal(), field);
	}
}

void SparsePolynomial::addModifiedPoly(SparsePolynomial& poly, FieldElt& mod, Field* field) {
	for (poly.iterStart(); !poly.iterEnd(); poly.iterNext()) {
		FieldElt val;
		field->mul(*poly.curRootVal(), mod, val);
		this->addNonZeroRoot(poly.curRootID(), val, field);
	}
}

bool SparsePolynomial::equal(Field* field, SparsePolynomial &other) {
	for (NonZeroRoots::iterator iter = this->nonZeroRoots.begin();
		   iter != nonZeroRoots.end();
			 iter++) {
		// Does he have this root?
		unsigned long rootID = iter->first;
		NonZeroRoots::iterator result = other.nonZeroRoots.find(rootID);

		if (result != other.nonZeroRoots.end()) {
			if (!field->equal(iter->second->value, result->second->value)) {
				return false;	// Value at this root differs
			}
		} else {
			return false;		// He doesn't even have this root ID!
		}
	}
	return true;
}

unsigned int SparsePolynomial::size() {
	return (unsigned int)nonZeroRoots.size();
}

void SparsePolynomial::print(Field* field) {
	for (NonZeroRoots::iterator iter = this->nonZeroRoots.begin();
		  iter != nonZeroRoots.end();
			iter++) {
		printf("(%u, ", iter->first);
		field->print(iter->second->value);
		printf(") ");
	}
}