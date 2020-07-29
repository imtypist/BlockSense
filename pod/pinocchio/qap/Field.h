#pragma once

#include <string.h>
#include "Types.h"

#ifdef __FFLIB
class __declspec(dllexport) Field {
#else
class Field {
#endif
public:
	Field(field_desc_tc* field);
	~Field();

	// Returns a randomly selected non-zero field element
	// It's your responsibility to free it
	FieldElt* newRandomElt();

	// Sets an existing field element to a randomly selected non-zero field element
	void assignRandomElt(FieldElt* elt);

	// Returns an element never before returned by newFreshElt
	// It's your responsibility to free it
	FieldElt* newFreshElt();	

	// Sets an existing field element to a new, never returned value
	void assignFreshElt(FieldElt* elt);

	// Return a fresh copy of the element 1
	// It's your responsibility to free it
	FieldElt* newOne();

	// Set an existing field element to zero
	void zero(FieldElt& elt) { memset(&elt, 0, sizeof(FieldElt)); }
	static void Zero(FieldElt* elt) { memset(elt, 0, sizeof(FieldElt)); }
	void zero(FieldElt* eltArray, int count) { memset(eltArray, 0, sizeof(FieldElt) * count); }

	// Set an existing field element to one
	void one(FieldElt& elt) { memcpy(&elt, msfield->one, sizeof(FieldElt)); }

	unsigned int bit(int index, FieldElt& in);

	// Extracts count bits, starting at index and heading towards the MSB
	int getBits(FieldElt &elt, int index, int count);

	void modulus(FieldElt &elt);

	// Set an existing field element to the integer value i
	void set(FieldElt& a, int i);

	// c <- a*b
	void mul(const FieldElt& a, const FieldElt& b, FieldElt& c);

	// c <- a/b
	void div(const FieldElt& a, const FieldElt& b, FieldElt& c);

	// c <- a+b
	void add(const FieldElt& a, const FieldElt& b, FieldElt& c);

	// c <- a-b
	void sub(const FieldElt& a, const FieldElt& b, FieldElt& c);

	// c <- a^exp (where ^ is exponentiation)
	void exp(const FieldElt& a, int exp, FieldElt& r);

	// Is a < b over the integers?
	bool lt(FieldElt& a, FieldElt& b);

	// dst <- src
	inline void copy(const FieldElt& src, FieldElt& dst) { memcpy(dst, src, sizeof(FieldElt)); }

	// The != 0 supresses a compiler warning about casting a BOOL (int) to a bool
	inline bool equal(const FieldElt& f1, const FieldElt& f2) { return Kequal(f1, f2, msfield, PBIGCTX_PASS) != 0; }
	
	void print(const FieldElt& e);

	field_desc_tc* msfield;
	
private:	
	bigctx_t BignumCtx;
	bigctx_t* pbigctx;
	digit_t mulTempSpace[8];
	digit_t divTempSpace[50];

	FieldElt ctr;
};
