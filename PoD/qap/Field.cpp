#include <assert.h>
#include <stdio.h>
#include <math.h>
#include "Field.h"

Field::Field(field_desc_tc* field) {
	msfield = field;
	assert(msfield->elng == sizeof(FieldElt) / sizeof(digit_t));	// Make sure our size assumptions are correct

	pbigctx = &this->BignumCtx;
	memset(this->pbigctx, 0, sizeof(bigctx_t));

	copy(*((FieldElt*)field->one), ctr);
}

Field::~Field() {
	
}

void Field::print(const FieldElt& e) {
	/*
	// Useful for printing 32-bit values when debugging
	FieldElt zero;
	FieldElt val;
	this->zero(zero);

	if (e[3] != 0) {
		printf("-");
		this->sub(zero, e, val);
	} else {
		this->copy(e, val);
	}
	
	for (int i = sizeof(FieldElt)/sizeof(digit_t) - 1; i >= 0; i--) {
		printf("%llx", val[i]);
	}*/
	for (int i = sizeof(FieldElt)/sizeof(digit_t) - 1; i >= 0; i--) {
		printf("%llx", e[i]);
	}
}

FieldElt* Field::newOne() { 
	FieldElt* ret = (FieldElt*) new FieldElt;
	copy(*((FieldElt*)msfield->one), *ret);
	return ret;
}

void Field::modulus(FieldElt &elt) {
	memcpy(&elt, msfield->modulo->modulus, sizeof(FieldElt) * sizeof(digit_t));
}

// c <- a*b
void Field::mul(const FieldElt& a, const FieldElt& b, FieldElt& c) {
	assert(sizeof(mulTempSpace) >= msfield->ndigtemps_mul * sizeof(digit_t));
	BOOL OK = Kmul(a, b, c, msfield, mulTempSpace, PBIGCTX_PASS);
	assert(OK);
}

// c <- a / b
void Field::div(const FieldElt& a, const FieldElt& b, FieldElt& c) {
	assert(sizeof(divTempSpace) >= msfield->ndigtemps_arith * sizeof(digit_t));
	BOOL OK = Kdiv(a, b, c, msfield, divTempSpace, PBIGCTX_PASS);
	assert(OK);
}

// c <- a+b
void Field::add(const FieldElt& a, const FieldElt& b, FieldElt& c) {
	BOOL OK = Kadd(a, b, c, msfield, PBIGCTX_PASS);
	assert(OK);
}

// c <- a-b
void Field::sub(const FieldElt& a, const FieldElt& b, FieldElt& c) {
	BOOL OK = Ksub(a, b, c, msfield, PBIGCTX_PASS);
	assert(OK);
}

// c <- a^exp (where ^ is exponentiation)
void Field::exp(const FieldElt& a, int exp, FieldElt& r) {
	FieldElt fexp;
	set(fexp, exp);
	BOOL OK = Kexpon(a, fexp, sizeof(FieldElt)/sizeof(digit_t), r, msfield, PBIGCTX_PASS);
	assert(OK);
}

// Is a < b over the integers?
bool Field::lt(FieldElt& a, FieldElt& b) {
	int num_digits_per_fieldelt = sizeof(FieldElt) / sizeof(digit_t);
	for (int i = 0; i < num_digits_per_fieldelt; i++) {
		int index = num_digits_per_fieldelt - i - 1;
		if (a[index] < b[index]) {
			return true;
		}
	}
	return false;
}


unsigned int Field::bit(int index, FieldElt& in) {
	if (index > 254) {
		return 0;
	}

	int numDigitBits = sizeof(digit_t) * 8;
	int digitIndex = index / numDigitBits;
	int intraDigitIndex = index % numDigitBits;

	digit_t mask = ((digit_t)1) << intraDigitIndex;

	return (mask & in[digitIndex]) > 0 ? 1 : 0;
}

void Field::set(FieldElt& a, int i) {
	BOOL OK = Kimmediate(i, a, msfield, PBIGCTX_PASS);
	assert(OK);
}

// Extracts count bits, starting at index and heading towards the MSB
int Field::getBits(FieldElt &elt, int index, int count) {	
	const int digitSize = sizeof(digit_t)*8;
	assert(count <= digitSize);

	int interDigitIndex = index / digitSize;
	int intraDigitIndex = index % digitSize;
	int numBitsToGet = min(count, digitSize - intraDigitIndex);	// Don't try to take more bits than remain in this digit

	int mask = (1 << numBitsToGet) - 1;  // mask = 000000111, where # of 1s = numBitsToGet

	digit_t shiftedDigit = (digit_t) (elt[interDigitIndex] >> intraDigitIndex);

	int ret = mask & shiftedDigit;

	if (numBitsToGet < count) {	// We were at the boundary of the prev. digit, so grab remaining bits from the next digit
		int numBitsGot = numBitsToGet;
		numBitsToGet = count - numBitsToGet;
		mask = (1 << numBitsToGet) - 1;  // mask = 000000111, where # of 1s = numBitsToGet
		ret |= (elt[interDigitIndex+1] & mask) << numBitsGot;	 // Make room for the bits we did get
	}

	return ret;
}


// Returns a randomly selected non-zero field element
// It's your responsibility to free it
FieldElt* Field::newRandomElt() {
	FieldElt* ret = (FieldElt*) new FieldElt;
	assignRandomElt(ret);
	return ret;
}

// Sets an existing field element to a randomly selected non-zero field element
void Field::assignRandomElt(FieldElt* elt) {
#ifdef DEBUG_RANDOM
	assignFreshElt(elt);
#else
	BOOL OK = Krandom_nonzero((digit_t*)elt, msfield, PBIGCTX_PASS);
	assert(OK);
#endif // DEBUG_RANDOM
}

void Field::assignFreshElt(FieldElt* elt) {
	copy(ctr, *elt);
	add(ctr, *((FieldElt*)msfield->one), ctr);	
}


