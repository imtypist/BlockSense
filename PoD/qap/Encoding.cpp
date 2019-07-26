#include <assert.h>
#include "Encoding.h"
#include <bitset>
#include <intrin.h>
#include "Poly.h"

#pragma intrinsic(_BitScanReverse)

#ifndef DEBUG_ENCODING
Encoding::Encoding() {
	pbigctx = &this->BignumCtx;
	memset(this->pbigctx, 0, sizeof(bigctx_t));

	// Setup the pairing parameters
	BOOL OK = TRUE;
	const char *bnname = named_bn[0].name;
	const char *bninfo = *(named_bn[0].bninfo);               // Pairing info 
    
	memset((void*)&bn, 0, sizeof(bn));                        
	OK = BN_build(bninfo, &bn, PBIGCTX_PASS);				  // Set up pairing struct
	assert(OK);	

	// Create a field to represent operations in the exponent
	p = new mp_modulus_t; 

	OK = create_modulus(bn.BaseCurve.gorder, bn.BaseCurve.lnggorder, FROM_LEFT, p, PBIGCTX_PASS);
	assert(OK);

	msExponentField = new field_desc_t; 

	OK = Kinitialize_prime(p, msExponentField, PBIGCTX_PASS);
	assert(OK);

	srcField = new Field(msExponentField);
	Poly::Initialize(srcField);

	lG = NULL;
	rG = NULL;

	// Build projective versions of the two curves' generators
	FieldElt zero, one;
	srcField->zero(zero);
	srcField->one(one);

	OK = ecaffine_projectivize(bn.BaseCurve.generator, lGen, &bn.BaseCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);

	OK = ecaffine_projectivize(bn.TwistCurve.generator, rGen, &bn.TwistCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);

	// Create a representation of zero, since we use it a lot
	OK = ecproj_single_exponentiation(lGen, zero, bn.pbits, bn.BaseCurve.lnggorder, lZero, &bn.BaseCurve, PBIGCTX_PASS);
	assert(OK);
	OK = ecproj_single_exponentiation(rGen, zero, bn.pbits, bn.TwistCurve.lnggorder, rZero, &bn.TwistCurve, PBIGCTX_PASS);
	assert(OK);

	numMults = 0;
	numSquares = 0;

	this->num_L_addsub = 0;
  this->num_L_double = 0;
  this->num_L_constmul = 0;
	this->num_R_addsub = 0;
  this->num_R_double = 0;
  this->num_R_constmul = 0;
  this->num_pair = 0;
}

Encoding::~Encoding() {
	delete srcField;
	delete msExponentField;
	uncreate_modulus(p, PBIGCTX_PASS);
	delete p;
	BN_free(&bn, PBIGCTX_PASS);

	deletePrecomputePowers();
	Poly::Cleanup();
}

void Encoding::reset_stats()
{
	this->num_L_addsub = 0;
  this->num_L_double = 0;
  this->num_L_constmul = 0;
	this->num_R_addsub = 0;
  this->num_R_double = 0;
  this->num_R_constmul = 0;
  this->num_pair = 0;  
}

void Encoding::print_stats()
{
  printf("addsub: %d (%d L, %d R)\n", this->num_L_addsub + this->num_R_addsub, this->num_L_addsub, this->num_R_addsub);
	printf("double: %d (%d L, %d R)\n", this->num_L_double + this->num_R_double, this->num_L_double, this->num_R_double);
  printf("constmul: %d (%d L, %d R)\n", this->num_L_constmul + this->num_R_constmul, this->num_L_constmul, this->num_R_constmul);
  printf("Pair: %d\n", this->num_pair);
}

void Encoding::deletePrecomputePowers() {
	if (lG != NULL) {
		for (int i = 0; i < v; i++) {
			delete [] lG[i];
			delete [] rG[i];
		}

		delete [] lG;
		delete [] rG;

		lG = NULL;
		rG = NULL;
	}
}

const int num_exponent_bits = sizeof(FieldElt)*8 - 1;	// BN_curve only uses 254 bits
LEncodedElt lPowersSlow[num_exponent_bits];	// powers[i] = x^{2^{i-1}}
REncodedElt rPowersSlow[num_exponent_bits];	// powers[i] = x^{2^{i-1}}
LEncodedElt lPowersFast[num_exponent_bits];	// powers[i] = x^{2^{i*a}}
REncodedElt rPowersFast[num_exponent_bits];	// powers[i] = x^{2^{i*a}}
bool precomputedPowersSlow = false;
bool precomputedPowersFast = false;

void Encoding::precomputePowersSlow() {
	if (precomputedPowersSlow) { return; } // Already precomputed

	// Handle the first two easy cases
	zero(lPowersSlow[0]);
	zero(rPowersSlow[0]);

	one(lPowersSlow[1]);
	one(rPowersSlow[1]);
	
	for (int i = 2; i < num_exponent_bits; i++) {
		// x^{2^i} = (x^{2^{i-1}})^2
		doubleIt(lPowersSlow[i-1], lPowersSlow[i]);
		doubleIt(rPowersSlow[i-1], rPowersSlow[i]);

		testOnCurve(lPowersSlow[i]);
		testOnCurve(rPowersSlow[i]);
	}

	precomputedPowersSlow = true;
}

unsigned long find_msb(unsigned long x) {
	if (x == 0) {
		return -1;
	} else {
		unsigned long ret;
		_BitScanReverse(&ret, x);
		return ret;
	}

#ifdef GCC
	if (x == 0) {
		return 0;
	} else {
		return sizeof(unsigned long) * 8 - __builtin_clz(x);
	}
#endif
}

//#ifdef _DEBUG
//// Pick something smaller and faster
//const int h = 4;
//const int v = 4;
//#else
//const int h = 16;
//const int v = 16;
//#endif
//
//const int a = (num_exponent_bits - 1) / h + 1;   // (int) ceil(num_exponent_bits / (float) h);		
//const int b = (a - 1) / v + 1;					 // (int) ceil(a / (float) v);
//
//// G[0][i] = g_{h-1}^{i_{h-1}} * g_{h-2}^{i_{h-2}} * ... * g_1^{i_1} * g_0^{i_0}
//LEncodedElt lG[v][1 << h];
//REncodedElt rG[v][1 << h];

/*
	Doesn't always seem to choose the optimal value of h.  

	h  Pre   LEnc  REnc  Total
	6	0.15	0.062	0.258	0.47
	7	0.022	0.051	0.218	0.291
	8	0.032	0.055	0.185	0.272   <- Actual min
	9	0.061	0.041	0.197	0.299
	10	0.118	0.037	0.153	0.308  <- Alg below choses this
	11	0.253	0.036	0.144	0.433
*/

void Encoding::allocatePreCalcMem(long long numExps, int maxMemGb, bool precompFree) {
	double maxMem = (double) maxMemGb * (((long long)1)<<30);

	if (precompFree) {
		// Build the largest table that will fit in memory
		for (h = 1; h < num_exponent_bits; h++) {
			long long twoToH = ((long long)1)<<h;
			long long storage = (twoToH - 1) * h * (sizeof(LEncodedElt) + sizeof(REncodedElt));
			if (storage > maxMem) {
				h--;
				break;				
			} 
		}
		v = h;
	} else {
		// Numerically minimize the expected number of multiplications, subject to the memory limit defined above
		long long minMults = INT64_MAX;
		int minH = -1;
		for (int hOption = 1; hOption < num_exponent_bits; hOption++) {
			long long twoToH = ((long long)1)<<hOption;
			long long storage = (long long)(twoToH - 1)*(long long)hOption * (long long)(sizeof(LEncodedElt) + sizeof(REncodedElt));
			if (storage > maxMem) {
				break;
			}

			// See qsp-practical/notes/optimizing-exp.txt for details
			//long long numMults = (int)((hOption-1)/(float)hOption * num_exponent_bits/2.0 + (1<<(hOption-1))*(hOption+1) + numExps*(num_exponent_bits*hOption + 1.5*num_exponent_bits - 2 * hOption*hOption) / (float)(hOption*hOption));
			int a = (int)ceil(num_exponent_bits / (float) hOption);
			int b = (int)ceil(num_exponent_bits / (float)(hOption*hOption));
		
			long long numMults = (long long) (numExps * (((twoToH-1)/(float)twoToH) * a + 1.5*b - 2)		// For the actual exponentations
													+ (hOption-1)*a/2.0 + twoToH * (hOption+1)/2.0);
			if (numMults < minMults) {
				minH = hOption;
				minMults = numMults;
			}
		}

		h = minH;
		v = minH;
	}

	printf("Chose h=v=%d for preallocation optimization\n", h);

	a = (int)ceil(num_exponent_bits / (float) h);
	b = (int) ceil(a / (float) v);

	lG = new LEncodedElt*[v];
	rG = new REncodedElt*[v];

	for (int i = 0; i < v; i++) {
		lG[i] = new LEncodedElt[((long long)1)<<h];
		rG[i] = new REncodedElt[((long long)1)<<h];
		//printf("Allocated lots of memory %d\n", i); fflush(stdout);		 
		//if (i == 15) {
		//	for (int j = 0; j < 10; j++) {
		//		Sleep(5000);
		//		printf("%d ", 10-j); fflush(stdout);
		//	}
		//}
	}	
}


// Implements more flexible exponentiation with precomputation, C.H.Lim and P.J.Lee, Advances in Cryptology-CRYPTO'94, http://dasan.sejong.ac.kr/~chlim/pub/crypto94.ps
// which is a special case of Pippenger's algorithm, according to http://cr.yp.to/papers/pippenger.pdf
void Encoding::precomputePowersFast(long long numExps, int maxMemGb, bool precompFree) {
	if (precomputedPowersFast) { return; } // Already precomputed

	allocatePreCalcMem(numExps, maxMemGb, precompFree);

	//printf("Precomputing powersFast with h = %d, v = %d\n", h, v);
	printf("Size of precomputed table = %d x %d = %d {L,R}EncodedElts = %.01f MB\n", v, 1 << h, v * (1<<h), v * (1<<h) * (sizeof(LEncodedElt) + sizeof(REncodedElt)) / (1024.0 * 1024));

	// Do the first one directly
	one(lPowersFast[0]);
	one(rPowersFast[0]);

	// Calculate x_i = x^{2^{i*a}}.  Takes (p-1)*a squarings
	for (int i = 1; i < h; i++) {
		for (int j = 0; j < a; j++) {
			if (j == 0) {
				doubleIt(lPowersFast[i-1], lPowersFast[i]); 
				doubleIt(rPowersFast[i-1], rPowersFast[i]); 
			} else {
				doubleIt(lPowersFast[i], lPowersFast[i]); 
				doubleIt(rPowersFast[i], rPowersFast[i]); 
			}
		}
	}
	
	// Handle G[0][0] directly
	zero(lG[0][0]);
	zero(rG[0][0]);

	for (int i = 1; i < (1 << h); i++) {
		int upperMostOneBit = find_msb(i);

		this->copy(lPowersFast[upperMostOneBit], lG[0][i]);
		this->copy(rPowersFast[upperMostOneBit], rG[0][i]);

		// Toggle the msb, and use the rest as an index into previous G calculations
		int lowerBits = (i ^ (1 << upperMostOneBit));
		if (i != 0 && lowerBits != 0) {
			add(lG[0][i], lG[0][lowerBits] , lG[0][i]);
			add(rG[0][i], rG[0][lowerBits] , rG[0][i]);
		}
	}

	for (int j = 1; j < v; j++) {
		for (int i = 0; i < (1 << h); i++) {
			// G[j][i] = (G[j-1][i])^{2^b}
			for (int k = 0; k < b; k++) {
				if (k == 0) {
					doubleIt(lG[j-1][i], lG[j][i]); 
					doubleIt(rG[j-1][i], rG[j][i]); 
				} else {
					doubleIt(lG[j][i], lG[j][i]);
					doubleIt(rG[j][i], rG[j][i]);
				}
			}
		}
	}

	precomputedPowersFast = true;
}


int Encoding::computeIndex(FieldElt& in, int j, int k) {
	int index = 0;

	for (int i = h - 1; i >= 0; i--) {
		int exp_index = i*a + b*j + k;

		if (b * j + k < a) {	// Otherwise, we've exceeded the length of this a-length block
			index <<= 1;
			index |= srcField->bit(exp_index, in);
		}
	}

	return index;
}


// Raise the generator of the base group to the element provided: out <- g_1^in
void Encoding::encode(FieldElt& in, LEncodedElt& out) {
	assert(precomputedPowersFast);
	copy(lG[0][0], out);		// out <- g^0 = 1
	
	for (int k = b - 1; k >= 0; k--) {
		if (k != b - 1) {
			doubleIt(out, out);		// No point the first time around
		}
		for (int j = v - 1; j >= 0; j--) {
			int index = computeIndex(in, j, k);
			if (index != 0) {	// lG[j][0] = g^0 = 1
				add(out, lG[j][index], out);
			}
		}
	}

	testOnCurve(out);

#ifdef DEBUG_EC
	// Raise the generator of the base group to the element provided: g1^in
	LEncodedElt slowOutAff;
	BOOL OK = ecaffine_exponentiation_via_multi_projective(bn.BaseCurve.generator, in, bn.pbits, bn.BaseCurve.lnggorder, slowOutAff, &bn.BaseCurve, PBIGCTX_PASS);
	assert(OK);

	LEncodedElt fastOutAff;
	affinize(out, fastOutAff);	

	assert(ecaffine_equal(slowOutAff, fastOutAff, &bn.BaseCurve, PBIGCTX_PASS));
#endif
}

// Raise the generator of the base group to the element provided: out <- g_1^in
void Encoding::encode(FieldElt& in, REncodedElt& out) {
	assert(precomputedPowersFast);
	copy(rG[0][0], out);		// out <- 1
	testOnCurve(out);

	int exponentIndex = -1;	

	for (int k = b - 1; k >= 0; k--) {
		if (k != b - 1) {
			doubleIt(out, out); 
		}
		for (int j = v - 1; j >= 0; j--) {
			int index = computeIndex(in, j, k);
			if (index != 0) {	// rG[j][0] = g^0 = 1
				add(out, rG[j][index], out);
			}
		}
	}

	testOnCurve(out);

#ifdef DEBUG_EC
	// Raise the generator of the base group to the element provided: g1^in
	REncodedElt slowOutAff;
	BOOL OK = ecaffine_exponentiation_via_multi_projective(bn.TwistCurve.generator, in, bn.pbits, bn.TwistCurve.lnggorder, slowOutAff, &bn.TwistCurve, PBIGCTX_PASS);
	assert(OK);

	REncodedElt fastOutAff;
	affinize(out, fastOutAff);

	assert(ecaffine_equal(slowOutAff, fastOutAff, &bn.TwistCurve, PBIGCTX_PASS));
#endif
}


#ifdef PIPPINGER
void Encoding::precomputePowers2() {
	digit_t temp[260];
	assert(bn.BaseCurve.ndigtemps <= sizeof(temp));
	assert(bn.TwistCurve.ndigtemps <= sizeof(temp));

	if (precomputedPowers) { return; } // Already precomputed

	FieldElt one;
	srcField->one(one);

	LEncodedElt lGen;
	REncodedElt rGen;

	BOOL OK = ecaffine_projectivize(bn.BaseCurve.generator, lGen, &bn.BaseCurve, temp, PBIGCTX_PASS);
	assert(OK);

	OK = ecaffine_projectivize(bn.TwistCurve.generator, rGen, &bn.TwistCurve, temp, PBIGCTX_PASS);
	assert(OK);

	// Do the first one directly
	OK = ecproj_single_exponentiation(lGen, one, bn.pbits, bn.BaseCurve.lnggorder, lPowers[0], &bn.BaseCurve, PBIGCTX_PASS);
	assert(OK);
	OK = ecproj_single_exponentiation(rGen, one, bn.pbits, bn.TwistCurve.lnggorder, rPowers[0], &bn.TwistCurve, PBIGCTX_PASS);
	assert(OK);

	//int h, v;
	//int a = (int) ceil(num_exponent_bits / (float) h);
	//int v = (int) ceil(a / (float) v);

	// Implement Pippenger's exponentiation algorithm, as described in http://cr.yp.to/papers/pippenger.pdf
	int k = 3;
	int p = (int) ceil(num_exponent_bits / (float) k);

	// Calculate x_i = x^{2^{k*i}}.  Takes (p-1)*k squarings
	assert(sizeof(lPowers) / sizeof(LEncodedElt) >= p);

	for (int i = 1; i < p; i++) {
		for (int j = 0; j < k; j++) {
			if (j == 0) {
				doubleIt(lPowers[i-1], lPowers[i]); 
			} else {
				doubleIt(lPowers[i], lPowers[i]); 
			}
		}
	}
	
	// Requires ceil(p/c)*(2^c - c - 1) mults
	int c = 5;
	LEncodedElt* foo = new LEncodedElt[ p / c + 1];
	for (int j = 0; c * j < p; j++) {
		this->one(foo[j]);
		for (int k = 0; k < c && c*j+k < p; k++) {
			int index = c*j+k;
			add(foo[j], lPowers[index], foo[j]);	// foo[j] *= x[index];
		}
	}

	precomputedPowers = true;
}

// Raise the generator of the base group to the element provided: out <- g_1^in
// Computes via at most k(ceil(p/c) - 1) + 2(k-1) mults
void Encoding::encode2(FieldElt& in, LEncodedElt& out) {
	assert(precomputedPowers);
	copy(lPowers[0], out);
	
	int exponentIndex = -1;
	const int num_digits = sizeof(FieldElt) / sizeof(digit_t);
	for (int i = 0; i < num_digits; i++) {
		digit_t tmp = in[i];
		exponentIndex = 1 + i * sizeof(digit_t) * 8;
		while (tmp > 0) {
			if (tmp & 0x01) { // tmp is odd
				add(out, lPowers[exponentIndex], out);
			}
			exponentIndex++;
			tmp = tmp >> 1;
		}
	}	

	testOnCurve(out);

#ifdef DEBUG_EC
	// Raise the generator of the base group to the element provided: g1^in
	LEncodedElt slowOutAff;
	BOOL OK = ecaffine_exponentiation_via_multi_projective(bn.BaseCurve.generator, in, bn.pbits, bn.BaseCurve.lnggorder, slowOutAff, &bn.BaseCurve, PBIGCTX_PASS);
	//BOOL OK = ecaffine_exponentiation_via_multi_projective(bn.BaseCurve.generator, in, bn.pbits, bn.BaseCurve.lnggorder, out, &bn.BaseCurve, PBIGCTX_PASS);
	assert(OK);

	LEncodedElt fastOutAff;
	affinize(out, fastOutAff);

	assert(ecaffine_equal(slowOutAff, fastOutAff, &bn.BaseCurve, PBIGCTX_PASS));
#endif
}

#endif

void Encoding::affinizeMany(LEncodedElt* in, LEncodedEltAff* out, int count, bool overWriteOk) {
	// ecprojective_affinize_many_in_place overwrites the input, which we may not want, so make a copy
	LEncodedElt* spreadOutResult = NULL;
	
	if (!overWriteOk) {
		spreadOutResult = new LEncodedElt[count];
		memcpy(spreadOutResult, in, count*sizeof(LEncodedElt));
	} else {
		spreadOutResult = in;
	}

	// Allocate some extra space
	int scratchLen = 2*count*sizeof(LEncodedElt)/sizeof(digit_t);
	digit_t* scratch = new digit_t[scratchLen];

	// Do it
	BOOL OK = ecprojective_affinize_many_in_place((digit_t*)spreadOutResult, count, &bn.BaseCurve, arithTempSpace, scratch, scratchLen, PBIGCTX_PASS);
	assert(OK);

	// ecprojective_affinize_many_in_place leaves the points at their original intervals.  Condense.
	for (int i = 0; i < count; i++) {
		memcpy(out + i, spreadOutResult + i, sizeof(LEncodedEltAff));
	}

	delete [] scratch;
	if (!overWriteOk) {
		delete [] spreadOutResult;
	}
}

void Encoding::affinizeMany(REncodedElt* in, REncodedEltAff* out, int count, bool overWriteOk) {
	// ecprojective_affinize_many_in_place overwrites the input, which we may not want, so make a copy
	REncodedElt* spreadOutResult = NULL;
	
	if (!overWriteOk) {
		spreadOutResult = new REncodedElt[count];
		memcpy(spreadOutResult, in, count*sizeof(REncodedElt));
	} else {
		spreadOutResult = in;
	}

	// Allocate some extra space
	int scratchLen = 2*count*sizeof(REncodedElt)/sizeof(digit_t);
	digit_t* scratch = new digit_t[scratchLen];

	// Do it
	BOOL OK = ecprojective_affinize_many_in_place((digit_t*)spreadOutResult, count, &bn.TwistCurve, arithTempSpace, scratch, scratchLen, PBIGCTX_PASS);
	assert(OK);

	// ecprojective_affinize_many_in_place leaves the points at their original intervals.  Condense.
	for (int i = 0; i < count; i++) {
		memcpy(out + i, spreadOutResult + i, sizeof(REncodedEltAff));
	}

	delete [] scratch;
	if (!overWriteOk) {
		delete [] spreadOutResult;
	}
}

void Encoding::affinize(LEncodedElt& in, LEncodedElt& out) {
	BOOL OK = ecprojective_affinize(in, out, &bn.BaseCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);
}

void Encoding::affinize(REncodedElt& in, REncodedElt& out) {
	BOOL OK = ecprojective_affinize(in, out, &bn.TwistCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);
}

void Encoding::affinize(LEncodedElt& in, LEncodedEltAff& out) {
	BOOL OK = ecprojective_affinize(in, out, &bn.BaseCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);
}

void Encoding::affinize(REncodedElt& in, REncodedEltAff& out) {
	BOOL OK = ecprojective_affinize(in, out, &bn.TwistCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);
}


void Encoding::projectivize(LEncodedEltAff& in, LEncodedElt& out) {
	BOOL OK = ecaffine_projectivize(in, out, &bn.BaseCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);
}

void Encoding::projectivize(REncodedEltAff& in, REncodedElt& out) {
	BOOL OK = ecaffine_projectivize(in, out, &bn.TwistCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);
}


void Encoding::projectivizeMany(LEncodedEltAff* in, LEncodedElt* out, int count) { 
	for (int i = 0; i < count; i++) {
		projectivize(in[i], out[i]);
	}
}

void Encoding::projectivizeMany(REncodedEltAff* in, REncodedElt* out, int count) { 
	for (int i = 0; i < count; i++) {
		projectivize(in[i], out[i]);
	}
}

void setToZero(digit_t* digits, int lenInBytes) {
	for (int i = 0; i < lenInBytes/sizeof(digit_t); i++) {
		digits[i] = 0;
	}
}

void Encoding::compress(LEncodedEltAff& in, LCondensedEncElt& out) {
	testOnCurve(in);

	digit_t* xCoeff = ((digit_t*)in)+0;
	digit_t* yCoeff = ((digit_t*)in)+4;

	// Special-case the point at infinity (since it isn't a solution to the curve equation)
	if (isZeroAff(in)) {
		setToZero(out, sizeof(LCondensedEncElt));		
		return;
	}

	// Compute x^3 + b, where b = 2
	digit_t expon = 3;
	digit_t xCubed[4];
	BOOL OK = Kexpon(xCoeff, &expon, 1, xCubed, &bn.BaseField, PBIGCTX_PASS);
	assert(OK);

	digit_t two[4];
	OK = Kimmediate(0, two, &bn.BaseField, PBIGCTX_PASS);
	assert(OK);
	OK = Kimmediate(2, two, &bn.BaseField, PBIGCTX_PASS);
	assert(OK);
	
	OK = Kadd(xCubed, two, xCubed, &bn.BaseField, PBIGCTX_PASS);
	assert(OK);

	// Compute positive root
	digit_t sqrtRhs[4];
	BOOL is_square;
	OK = Ksqrt(xCubed, sqrtRhs, &bn.BaseField, &is_square, PBIGCTX_PASS);
	assert(OK);
	assert(is_square);

#ifdef DEBUG_EC
	// Check the square root is correct
	digit_t tmps[2000];
	digit_t allegedRhs[4];
	OK = Kmul(sqrtRhs, sqrtRhs, allegedRhs, &bn.BaseField, tmps, PBIGCTX_PASS); 
	assert(OK);
	assert(Kequal(allegedRhs, xCubed, &bn.BaseField, PBIGCTX_PASS));	
#endif

	BOOL equal = Kequal(sqrtRhs, yCoeff, &bn.BaseField, PBIGCTX_PASS);
	
	// We use the top-most bit of X to store the bit indicating whether to use the negative root
	int lastDigit = sizeof(LEncodedEltAff)/sizeof(digit_t)/2-1;	// EncodedElt contains 2 field elements
	if (!equal) {
		// Set the top bit of the last digit of X to indicate that we use the "negative" root		
		xCoeff[lastDigit] |= ((digit_t)1) << 63;
	} else {
		// Make sure the top bit is actually clear, indicating the "positive" root		
		assert(0 == (xCoeff[lastDigit] & (((digit_t)1) << 63)));
	}

	memcpy(out, xCoeff, sizeof(LEncodedEltAff)/2);
}

// On the twist curve, we use y^2 = x^3 + (1 - i), and each point is represented as: a + i*b
void Encoding::compress(REncodedEltAff& in, RCondensedEncElt& out) {
	testOnCurve(in);
	digit_t* xCoeff = ((digit_t*)in)+0;
	digit_t* yCoeff = ((digit_t*)in)+8;

	// Special-case the point at infinity (since it isn't a solution to the curve equation)
	if (isZeroAff(in)) {
		setToZero(out, sizeof(RCondensedEncElt));
		return;
	}

	// Compute x^3 + (1 - i)
	digit_t expon = 3;
	digit_t xCubed[8];
	BOOL OK = Kexpon(xCoeff, &expon, 1, xCubed, &bn.TwistField, PBIGCTX_PASS);
	assert(OK);

	digit_t oneNegOne[8];	
	OK = Kimmediate(1, oneNegOne, &bn.BaseField, PBIGCTX_PASS);	// Put 1 into first slot
	assert(OK);
	OK = Knegate(oneNegOne, ((digit_t*)oneNegOne)+4, &bn.BaseField, PBIGCTX_PASS);	// Put -1 into the second slot
	assert(OK);
	
	OK = Kadd(xCubed, oneNegOne, xCubed, &bn.TwistField, PBIGCTX_PASS);
	assert(OK);

	// Compute a root
	digit_t sqrtRhs[8];
	BOOL is_square;
	OK = Ksqrt(xCubed, sqrtRhs, &bn.TwistField, &is_square, PBIGCTX_PASS);
	assert(OK);
	assert(is_square);

#ifdef DEBUG_EC
	// Check the square root is correct
	digit_t tmps[2000];
	digit_t allegedRhs[8];
	OK = Kmul(sqrtRhs, sqrtRhs, allegedRhs, &bn.TwistField, tmps, PBIGCTX_PASS); 
	assert(OK);
	assert(Kequal(allegedRhs, xCubed, &bn.TwistField, PBIGCTX_PASS));	
#endif

	// Compute the other root
	digit_t negSqrtRhs[8];
	OK = Knegate(sqrtRhs, negSqrtRhs, &bn.TwistField, PBIGCTX_PASS);
	assert(OK);

	BOOL equal = Kequal(sqrtRhs, yCoeff, &bn.TwistField, PBIGCTX_PASS);

	// Since we can't actually test for negative roots, and the library gives us a random one,
	// we distinguish between them based on which last digit is smaller
	bool equalsSmaller = (equal && (sqrtRhs[7] <= negSqrtRhs[7])) || (!equal && (sqrtRhs[7] > negSqrtRhs[7]));
	
	// We use the top-most bit of X to store the bit indicating whether to use the smaller root
	int lastDigit = sizeof(REncodedEltAff)/sizeof(digit_t)/2-1;	// EncodedElt contains 2 field elements
	if (equalsSmaller) {
		// Set the top bit of the last digit of X to indicate that we use the smaller root
		xCoeff[lastDigit] |= ((digit_t)1) << 63;
	} else {
		// Make sure the top bit is actually clear, indicating the larger root
		assert(!(xCoeff[lastDigit] & (((digit_t)1) << 63)));
	}

	memcpy(out, xCoeff, sizeof(REncodedEltAff)/2);
}

void Encoding::compress(LEncodedElt& in, LCondensedEncElt& out) {
	LEncodedEltAff affineIn;
	affinize(in, affineIn);
	compress(affineIn, out);
}

void Encoding::compress(REncodedElt& in, RCondensedEncElt& out) {
	REncodedEltAff affineIn;
	affinize(in, affineIn);
	compress(affineIn, out);
}

void Encoding::decompress(LCondensedEncElt& in, LEncodedElt& out) {	
	digit_t* xCoeff = ((digit_t*)out)+0;
	digit_t* yCoeff = ((digit_t*)out)+4;

	// Special-case the point at infinity (since it isn't a solution to the curve equation)
	if (isZero(in)) {
		this->zero(out);
		return;
	}

	// Check whether the top bit of X is set.  Indicates which root to use for Y
	int lastDigit = sizeof(LEncodedEltAff)/sizeof(digit_t)/2-1;	// EncodedElt contains 2 field elements
	bool useNegRoot = (in[lastDigit] & (((digit_t)1) << 63)) > 0;
	
	// Either way, clear out the top bit and copy the input into the x coefficient
	in[lastDigit] &= ( (((digit_t)1) << 63) - 1);
	memcpy(xCoeff, in, sizeof(LCondensedEncElt));

	// Compute x^3 + b, where b = 2
	digit_t expon = 3;
	digit_t xCubed[4];
	BOOL OK = Kexpon(xCoeff, &expon, 1, xCubed, &bn.BaseField, PBIGCTX_PASS);
	assert(OK);

	digit_t two[4];
	OK = Kimmediate(2, two, &bn.BaseField, PBIGCTX_PASS);
	assert(OK);
	OK = Kadd(xCubed, two, xCubed, &bn.BaseField, PBIGCTX_PASS);
	assert(OK);

	// Compute "positive" root and, if necessary, use it to compute the negative root
	BOOL is_square;
	OK = Ksqrt(xCubed, yCoeff, &bn.BaseField, &is_square, PBIGCTX_PASS);
	assert(OK);
	assert(is_square);

	if (useNegRoot) { 
		OK = Knegate(yCoeff, yCoeff, &bn.BaseField, PBIGCTX_PASS);	
		assert(OK);
	}

	projectivize(*((LEncodedEltAff*)out), out);
	testOnCurve(out);
}

// On the twist curve, we use y^2 = x^3 + (1 - i), and each point is represented as a + i*b
void Encoding::decompress(RCondensedEncElt& in, REncodedElt& out) {	
	digit_t* xCoeff = ((digit_t*)out)+0;
	digit_t* yCoeff = ((digit_t*)out)+8;

	// Special-case the point at infinity (since it isn't a solution to the curve equation)
	if (isZero(in)) {
		this->zero(out);		
		return;
	}

	// Check whether the top bit of X is set.  Indicates which root to use for Y
	int lastDigit = sizeof(REncodedEltAff)/sizeof(digit_t)/2-1;	// EncodedElt contains 2 field elements
	bool useSmallerRoot = (in[lastDigit] & (((digit_t)1) << 63)) > 0;
	
	// Either way, clear out the top bit and copy the input into the x coefficient
	in[lastDigit] &= ( (((digit_t)1) << 63) - 1);
	memcpy(xCoeff, in, sizeof(RCondensedEncElt));

	// Compute x^3 + (1 - i)
	digit_t expon = 3;
	digit_t xCubed[8];
	BOOL OK = Kexpon(xCoeff, &expon, 1, xCubed, &bn.TwistField, PBIGCTX_PASS);
	assert(OK);

	digit_t oneNegOne[8];	
	OK = Kimmediate(1, oneNegOne, &bn.BaseField, PBIGCTX_PASS);	// Put 1 into first slot
	assert(OK);
	OK = Knegate(oneNegOne, ((digit_t*)oneNegOne)+4, &bn.BaseField, PBIGCTX_PASS);	// Put -1 into the second slot
	assert(OK);
	
	OK = Kadd(xCubed, oneNegOne, xCubed, &bn.TwistField, PBIGCTX_PASS);
	assert(OK);	

	// Compute a root and use it to compute the other root
	BOOL is_square;
	OK = Ksqrt(xCubed, yCoeff, &bn.TwistField, &is_square, PBIGCTX_PASS);
	assert(OK);
	assert(is_square);

	digit_t otherRoot[8];
	OK = Knegate(yCoeff, otherRoot, &bn.TwistField, PBIGCTX_PASS);	

#ifdef DEBUG_EC
	// Check the other square root is correct
	digit_t tmps[2000];
	digit_t allegedRhs[8];
	OK = Kmul(otherRoot, otherRoot, allegedRhs, &bn.TwistField, tmps, PBIGCTX_PASS); 
	assert(OK);
	assert(Kequal(allegedRhs, xCubed, &bn.TwistField, PBIGCTX_PASS));	
#endif

	if ((useSmallerRoot && otherRoot[7] <= yCoeff[7]) || 
			(!useSmallerRoot && otherRoot[7] > yCoeff[7]) ) {
		memcpy(yCoeff, otherRoot, 8*sizeof(digit_t));
	}	

	projectivize(*((REncodedEltAff*)out), out);
	testOnCurve(out);
}

// overWriteOk = true if okay to overwrite in
void Encoding::compressMany(LEncodedElt* in, LCondensedEncElt* out, int count, bool overWriteOk) {
	LEncodedEltAff* aff = NULL;
	if (overWriteOk) {
		// Affinize in place
		aff = (LEncodedEltAff*) in;
	} else {
		// Make room
		aff = new LEncodedEltAff[count];
	}

	affinizeMany(in, aff, count, overWriteOk);

	// Compress each affine point individually
	for (int i = 0; i < count; i++) {
		compress(aff[i], out[i]);
	}

	// Clean up, if necessary
	if (!overWriteOk) {
		delete [] aff;
	}
}

// overWriteOk = true if okay to overwrite in
void Encoding::compressMany(REncodedElt* in, RCondensedEncElt* out, int count, bool overWriteOk) {
	REncodedEltAff* aff = NULL;
	if (overWriteOk) {
		// Affinize in place
		aff = (REncodedEltAff*) in;
	} else {
		// Make room
		aff = new REncodedEltAff[count];
	}

	affinizeMany(in, aff, count, overWriteOk);

	// Compress each affine point individually
	for (int i = 0; i < count; i++) {
		compress(aff[i], out[i]);
	}

	// Clean up, if necessary
	if (!overWriteOk) {
		delete [] aff;
	}
}

void Encoding::decompressMany(LCondensedEncElt* in, LEncodedElt* out, int count) {
	for (int i = 0; i < count; i++) {
		decompress(in[i], out[i]);
	}
}

void Encoding::decompressMany(RCondensedEncElt* in, REncodedElt* out, int count) {
	for (int i = 0; i < count; i++) {
		decompress(in[i], out[i]);
	}
}


void Encoding::doubleIt(LEncodedElt& a, LEncodedElt& b) {
	numSquares++;
	testOnCurve(a);
	BOOL OK = ecprojective_double(a, b, &bn.BaseCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);
	testOnCurve(b);
	this->num_L_double++;
}

void Encoding::doubleIt(REncodedElt& a, REncodedElt& b) {
	numSquares++;
	testOnCurve(a);
	BOOL OK = ecprojective_double(a, b, &bn.TwistCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);
	testOnCurve(b);
	this->num_R_double++;
}

void Encoding::testOnCurve(LEncodedElt& elt) {
#ifdef _DEBUG
	digit_t temp[260];
	BOOL OK = ecprojective_on_curve(elt, &bn.BaseCurve, "I'm on a curve", temp, PBIGCTX_PASS);
	assert(OK);
#endif
}

void Encoding::testOnCurve(REncodedElt& elt) {
#ifdef _DEBUG
	digit_t temp[260];
	BOOL OK = ecprojective_on_curve(elt, &bn.TwistCurve, "I'm on a curve", temp, PBIGCTX_PASS);
	assert(OK);
#endif
}


void Encoding::testOnCurve(LEncodedEltAff& elt) {
#ifdef _DEBUG
	digit_t temp[260];
	BOOL OK = ecaffine_on_curve(elt, &bn.BaseCurve, "I'm on a curve", temp, PBIGCTX_PASS);
	assert(OK);
#endif
}

void Encoding::testOnCurve(REncodedEltAff& elt) {
#ifdef _DEBUG
	digit_t temp[260];
	BOOL OK = ecaffine_on_curve(elt, &bn.TwistCurve, "I'm on a curve", temp, PBIGCTX_PASS);
	assert(OK);
#endif
}

void Encoding::print(LEncodedElt& elt) {
	for (int i = 0; i < sizeof(LEncodedElt)/sizeof(digit_t); i++) {
		printf("%llx ", elt[i]);
	}
}

void Encoding::print(REncodedElt& elt) {
	for (int i = 0; i < sizeof(REncodedElt)/sizeof(digit_t); i++) {
		printf("%llx ", elt[i]);
	}
}


#define PRECOMPUTE

// Raise the generator of the base group to the element provided: out <- g_1^in
void Encoding::encodeSlow(FieldElt& in, LEncodedElt& out) {
	assert(precomputedPowersSlow);
	copy(lPowersSlow[0], out);
	
	int exponentIndex = -1;
	const int num_digits = sizeof(FieldElt) / sizeof(digit_t);
	for (int i = 0; i < num_digits; i++) {
		digit_t tmp = in[i];
		exponentIndex = 1 + i * sizeof(digit_t) * 8;
		while (tmp > 0) {
			if (tmp & 0x01) { // tmp is odd
				if (exponentIndex >= num_exponent_bits) { break; }
				add(out, lPowersSlow[exponentIndex], out);
			}
			exponentIndex++;
			tmp = tmp >> 1;
		}
	}	

	testOnCurve(out);

#ifdef DEBUG_EC
	// Raise the generator of the base group to the element provided: g1^in
	LEncodedElt slowOutAff;
	BOOL OK = ecaffine_exponentiation_via_multi_projective(bn.BaseCurve.generator, in, bn.pbits, bn.BaseCurve.lnggorder, slowOutAff, &bn.BaseCurve, PBIGCTX_PASS);
	//BOOL OK = ecaffine_exponentiation_via_multi_projective(bn.BaseCurve.generator, in, bn.pbits, bn.BaseCurve.lnggorder, out, &bn.BaseCurve, PBIGCTX_PASS);
	assert(OK);

	LEncodedElt fastOutAff;
	affinize(out, fastOutAff);
	
	assert(ecaffine_equal(slowOutAff, fastOutAff, &bn.BaseCurve, PBIGCTX_PASS));
#endif
}


void Encoding::encodeSlow(FieldElt& in, REncodedElt& out) {
//#ifdef PRECOMPUTE
	assert(precomputedPowersSlow);
	copy(rPowersSlow[0], out);
	
	int exponentIndex = -1;
	const int num_digits = sizeof(FieldElt) / sizeof(digit_t);
	for (int i = 0; i < num_digits; i++) {
		digit_t tmp = in[i];
		exponentIndex = 1 + i * sizeof(digit_t) * 8;
		while (tmp > 0) {
			if (tmp & 0x01) { // tmp is odd
				add(out, rPowersSlow[exponentIndex], out);
			}
			exponentIndex++;
			tmp = tmp >> 1;
		}
	}	

	testOnCurve(out);

#ifdef DEBUG_EC
	// Raise the generator of the base group to the element provided: g1^in
	REncodedElt slowOutAff;
	BOOL OK = ecaffine_exponentiation_via_multi_projective(bn.TwistCurve.generator, in, bn.pbits, bn.TwistCurve.lnggorder, slowOutAff, &bn.TwistCurve, PBIGCTX_PASS);
	//BOOL OK = ecaffine_exponentiation_via_multi_projective(bn.TwistCurve.generator, in, bn.pbits, bn.TwistCurve.lnggorder, out, &bn.TwistCurve, PBIGCTX_PASS);
	assert(OK);

	REncodedElt fastOutAff;
	affinize(out, fastOutAff);
	
	assert(ecaffine_equal(slowOutAff, fastOutAff, &bn.TwistCurve, PBIGCTX_PASS));
#endif
}


// Raise the generator of the base group to the element provided: out <- g_1^in
void Encoding::encodeSlowest(FieldElt& in, LEncodedElt& out) {
	BOOL OK = ecproj_single_exponentiation(lGen, in, bn.pbits, bn.BaseCurve.lnggorder, out, &bn.BaseCurve, PBIGCTX_PASS);
	assert(OK);
}

// Raise the generator of the base group to the element provided: out <- g_1^in
void Encoding::encodeSlowest(FieldElt& in, REncodedElt& out) {
	BOOL OK = ecproj_single_exponentiation(rGen, in, bn.pbits, bn.TwistCurve.lnggorder, out, &bn.TwistCurve, PBIGCTX_PASS);
	assert(OK);
}


void Encoding::zero(LEncodedElt& a) { 
	copy(lZero, a);	// Use our precomputed value	
}

void Encoding::zero(REncodedElt& a) { 
	copy(rZero, a);	// Use our precomputed value
}

void Encoding::one(LEncodedElt& a) { 
	copy(lGen, a);	// Use our precomputed value	
}

void Encoding::one(REncodedElt& a) { 
	copy(rGen, a);	// Use our precomputed value
}

void Encoding::mul(LEncodedElt &a, REncodedElt &b, EncodedProduct& r) {
	pair(a, b, r);
}

void Encoding::add(EncodedProduct& a, EncodedProduct& b, EncodedProduct& r) {
	digit_t tmps[500];
	assert(bn.ExtField.ndigtemps_arith <= sizeof(tmps));

	BOOL OK = K12mul(a, b, r, &bn, tmps, PBIGCTX_PASS);		
	assert(OK);
}

void Encoding::sub(EncodedProduct& a, EncodedProduct& b, EncodedProduct& r) {
	EncodedProduct tmp;
	digit_t tmps[500];
	assert(bn.ExtField.ndigtemps_arith <= sizeof(tmps));

	BOOL OK = K12inv(b, tmp, &bn, tmps, PBIGCTX_PASS);
	assert(OK);
	OK = K12mul(a, tmp, r, &bn, tmps, PBIGCTX_PASS);		
	assert(OK);
	//BOOL OK = Kdiv(a, b, r, &bn.ExtField, tmps, PBIGCTX_PASS);
	//assert(OK);
}

bool Encoding::equals(EncodedProduct& a, EncodedProduct& b) {
	return 0 != Kequal(a, b, &bn.ExtField, PBIGCTX_PASS);
}

// Multiplies the value encoded in g by the constant c.  Result goes in r.
// For pairings, we compute r <- g^c
void Encoding::mul(LEncodedElt& g, FieldElt& c, LEncodedElt& r) {		
#ifdef DEBUG_EC
	LEncodedElt gAff;
	affinize(g, gAff);
#endif

	testOnCurve(g);
	BOOL OK = ecproj_single_exponentiation(g, c, bn.pbits, bn.BaseCurve.lnggorder, r, &bn.BaseCurve, PBIGCTX_PASS);
	assert(OK);
	testOnCurve(r);

#ifdef DEBUG_EC
	LEncodedElt slowResult, rAff;
	OK = ecaffine_exponentiation_via_multi_projective(gAff, c, bn.pbits, bn.BaseCurve.lnggorder, slowResult, &bn.BaseCurve, PBIGCTX_PASS);
	assert(OK);
	affinize(r, rAff);	
	assert(ecaffine_equal(rAff, slowResult, &bn.BaseCurve, PBIGCTX_PASS));
#endif
	this->num_L_constmul++;
}

// Multiplies the value encoded in g by the constant c.  Result goes in r.
// For pairings, we compute r <- g^c
void Encoding::mul(REncodedElt& g, FieldElt& c, REncodedElt& r) {	
#ifdef DEBUG_EC
	REncodedElt gAff;
	affinize(g, gAff);
#endif

	testOnCurve(g);
	BOOL OK = ecproj_single_exponentiation(g, c, bn.pbits, bn.TwistCurve.lnggorder, r, &bn.TwistCurve, PBIGCTX_PASS);
	assert(OK);
	testOnCurve(r);


#ifdef DEBUG_EC
	REncodedElt slowResult, rAff;
	OK = ecaffine_exponentiation_via_multi_projective(gAff, c, bn.pbits, bn.TwistCurve.lnggorder, slowResult, &bn.TwistCurve, PBIGCTX_PASS);
	assert(OK);
	affinize(r, rAff);
	assert(ecaffine_equal(rAff, slowResult, &bn.TwistCurve, PBIGCTX_PASS));
#endif
	this->num_R_constmul++;
}

// Computes r <- g_{a_i}^{b_i} for 0 <= i < len
void Encoding::multiMulOld(multi_exponent_t* multiExp, int len, LEncodedElt& r) {
#ifdef DEBUG_EC
	// Convert all of the input values to affine points
	LEncodedElt* vals = new LEncodedElt[len];
	multi_exponent_t* multiExpAff = new multi_exponent_t[len];
	for (int i = 0; i < len; i++) {
		multiExpAff[i].lng_bits = multiExp[i].lng_bits;
		multiExpAff[i].lng_pexp_alloc = multiExp[i].lng_pexp_alloc;
		multiExpAff[i].offset_bits = multiExp[i].offset_bits;
		multiExpAff[i].pexponent = multiExp[i].pexponent;

		affinize(*(LEncodedElt*)(multiExp[i].pbase), vals[i]);

		multiExpAff[i].pbase = vals[i];
	}
	// Do the multi exponentiation
	LEncodedElt slowResult;
	BOOL okay = ecaffine_exp_multi(multiExpAff, len, slowResult, &bn.BaseCurve, PBIGCTX_PASS);
	assert(okay);
#endif

	BOOL OK = ecproj_exp_multi_via_projective(multiExp, len, r, &bn.BaseCurve, PBIGCTX_PASS);
	assert(OK);
	testOnCurve(r);

#ifdef DEBUG_EC
	LEncodedElt rAff;
	affinize(r, rAff);

	assert(ecaffine_equal(rAff, slowResult, &bn.BaseCurve, PBIGCTX_PASS));

	delete [] vals;
	delete [] multiExpAff;
#endif 
}

void Encoding::multiMulOld(multi_exponent_t* multiExp, int len, REncodedElt& r) {
#ifdef DEBUG_EC
	// Convert all of the input values to affine points
	REncodedElt* vals = new REncodedElt[len];
	multi_exponent_t* multiExpAff = new multi_exponent_t[len];
	for (int i = 0; i < len; i++) {
		multiExpAff[i].lng_bits = multiExp[i].lng_bits;
		multiExpAff[i].lng_pexp_alloc = multiExp[i].lng_pexp_alloc;
		multiExpAff[i].offset_bits = multiExp[i].offset_bits;
		multiExpAff[i].pexponent = multiExp[i].pexponent;

		affinize(*(REncodedElt*)(multiExp[i].pbase), vals[i]);

		multiExpAff[i].pbase = vals[i];
	}
	// Do the multi exponentiation
	REncodedElt slowResult;
	BOOL okay = ecaffine_exp_multi(multiExpAff, len, slowResult, &bn.TwistCurve, PBIGCTX_PASS);
	assert(okay);
#endif

	BOOL OK = ecproj_exp_multi_via_projective(multiExp, len, r, &bn.TwistCurve, PBIGCTX_PASS);
	assert(OK);
	testOnCurve(r);

#ifdef DEBUG_EC
	REncodedElt rAff;
	affinize(r, rAff);
	assert(ecaffine_equal(rAff, slowResult, &bn.TwistCurve, PBIGCTX_PASS));

	delete [] vals;
	delete [] multiExpAff;
#endif 
}


// Adds two encoded elements.  Result goes in r.
void Encoding::add(LEncodedElt& a, LEncodedElt& b, LEncodedElt& r) {
	assert(sizeof(arithTempSpace) >= bn.BaseCurve.fdesc->ndigtemps_arith * 2 * sizeof(digit_t));
	testOnCurve(a);
	testOnCurve(b);
	numMults++;

#ifdef DEBUG_EC
	LEncodedElt aAff, bAff;
	affinize(a, aAff);
	affinize(b, bAff);
#endif

	BOOL OK = ecprojective_addsub_projective(a, b, r, 1, &bn.BaseCurve, arithTempSpace, PBIGCTX_PASS);		
	assert(OK);
	testOnCurve(r);

#ifdef DEBUG_EC
	LEncodedElt rAff, affResult;
	affinize(r, rAff);
	OK = ecaffine_addition(aAff, bAff, affResult, 1, &bn.BaseCurve, arithTempSpace, PBIGCTX_PASS);
	assert(OK);

	assert(ecaffine_on_curve(aAff, &bn.BaseCurve, "On a curve?", arithTempSpace, PBIGCTX_PASS));
	assert(ecaffine_on_curve(bAff, &bn.BaseCurve, "On a curve?", arithTempSpace, PBIGCTX_PASS));
	assert(ecaffine_on_curve(rAff, &bn.BaseCurve, "On a curve?", arithTempSpace, PBIGCTX_PASS));
	assert(ecaffine_on_curve(affResult, &bn.BaseCurve, "On a curve?", arithTempSpace, PBIGCTX_PASS));

	assert(ecaffine_equal(rAff, affResult, &bn.BaseCurve, PBIGCTX_PASS));
#endif
	this->num_L_addsub++;
}

// Adds two encoded elements.  Result goes in r.
void Encoding::add(REncodedElt& a, REncodedElt& b, REncodedElt& r) {
	numMults++;
	assert(sizeof(arithTempSpace) >= bn.TwistCurve.fdesc->ndigtemps_arith * 2 * sizeof(digit_t));
	testOnCurve(a);
	testOnCurve(b);

#ifdef DEBUG_EC
	digit_t tmps[260];
	REncodedElt aAff, bAff;
	affinize(a, aAff);
	affinize(b, bAff);
#endif

	BOOL OK = ecprojective_addsub_projective(a, b, r, 1, &bn.TwistCurve, arithTempSpace, PBIGCTX_PASS);		
	assert(OK);
	testOnCurve(r);

#ifdef DEBUG_EC
	REncodedElt rAff, affResult;
	affinize(r, rAff);
	OK = ecaffine_addition(aAff, bAff, affResult, 1, &bn.TwistCurve, tmps, PBIGCTX_PASS);
	assert(OK);
	assert(ecaffine_equal(rAff, affResult, &bn.TwistCurve, PBIGCTX_PASS));
#endif
	this->num_R_addsub++;
}

// Subtracts two encoded elements.  Result goes in r.
void Encoding::sub(LEncodedElt& a, LEncodedElt& b, LEncodedElt& r) {
	assert(sizeof(arithTempSpace) >= bn.BaseCurve.fdesc->ndigtemps_arith * 2 * sizeof(digit_t));
	testOnCurve(a);
	testOnCurve(b);

#ifdef DEBUG_EC
	digit_t tmps[260];
	LEncodedElt aAff, bAff;
	affinize(a, aAff);
	affinize(b, bAff);
#endif

	BOOL OK = ecprojective_addsub_projective(a, b, r, -1, &bn.BaseCurve, arithTempSpace, PBIGCTX_PASS);		
	assert(OK);
	testOnCurve(r);

#ifdef DEBUG_EC
	LEncodedElt rAff, affResult;
	affinize(r, rAff);
	OK = ecaffine_addition(aAff, bAff, affResult, -1, &bn.BaseCurve, tmps, PBIGCTX_PASS);
	assert(OK);
	assert(ecaffine_equal(rAff, affResult, &bn.BaseCurve, PBIGCTX_PASS));
#endif
	this->num_L_addsub++;
}

// Subtracts two encoded elements.  Result goes in r.
void Encoding::sub(REncodedElt& a, REncodedElt& b, REncodedElt& r) {
	assert(sizeof(arithTempSpace) >= bn.TwistCurve.fdesc->ndigtemps_arith * 2 * sizeof(digit_t));
	testOnCurve(a);
	testOnCurve(b);

#ifdef DEBUG_EC
	digit_t tmps[260];
	REncodedElt aAff, bAff;
	affinize(a, aAff);
	affinize(b, bAff);
#endif

	BOOL OK = ecprojective_addsub_projective(a, b, r, -1, &bn.TwistCurve, arithTempSpace, PBIGCTX_PASS);		
	assert(OK);
	testOnCurve(r);

#ifdef DEBUG_EC
	REncodedElt rAff, affResult;
	affinize(r, rAff);
	OK = ecaffine_addition(aAff, bAff, affResult, -1, &bn.TwistCurve, tmps, PBIGCTX_PASS);
	assert(OK);
	assert(ecaffine_equal(rAff, affResult, &bn.TwistCurve, PBIGCTX_PASS));
#endif
	this->num_R_addsub++;
}

bool isAllZero(digit_t* value, int sizeInBytes) {
	for (int i = 0; i < sizeInBytes / sizeof(digit_t); i++) {
		if (value[i] != 0) {
			return false;
		}
	}
	return true;
}

bool Encoding::isZero(LCondensedEncElt& elt) {
	return isAllZero((digit_t*)&elt, sizeof(LCondensedEncElt));
}

bool Encoding::isZero(RCondensedEncElt& elt) {
	return isAllZero((digit_t*)&elt, sizeof(RCondensedEncElt));
}

	
bool Encoding::isZeroAff(LEncodedEltAff& elt) {
	digit_t temp[260];
	assert(bn.BaseCurve.ndigtemps <= sizeof(temp));

	LEncodedElt eltProj;
	BOOL OK = ecaffine_projectivize(elt, eltProj, &bn.BaseCurve, temp, PBIGCTX_PASS);
	assert(OK);

	BOOL equal = ecprojective_equivalent(eltProj, lZero, &bn.BaseCurve, temp, PBIGCTX_PASS);
	
	return equal != 0;
}

bool Encoding::isZeroAff(REncodedEltAff& elt) {
	digit_t temp[260];
	assert(bn.TwistCurve.ndigtemps <= sizeof(temp));

	REncodedElt eltProj;
	BOOL OK = ecaffine_projectivize(elt, eltProj, &bn.TwistCurve, temp, PBIGCTX_PASS);
	assert(OK);

	BOOL equal = ecprojective_equivalent(eltProj, rZero, &bn.TwistCurve, temp, PBIGCTX_PASS);
	
	return equal != 0;
}


bool Encoding::isZero(LEncodedElt& elt) {
	digit_t temp[260];
	assert(bn.BaseCurve.ndigtemps <= sizeof(temp));
	testOnCurve(elt);

	BOOL equal = ecprojective_equivalent(elt, lZero, &bn.BaseCurve, temp, PBIGCTX_PASS);
	
	return equal != 0;
}

bool Encoding::isZero(REncodedElt& elt) {
	digit_t temp[260];
	assert(bn.TwistCurve.ndigtemps <= sizeof(temp));
	testOnCurve(elt);

	BOOL equal = ecprojective_equivalent(elt, rZero, &bn.TwistCurve, temp, PBIGCTX_PASS);
	
	return equal != 0;
}

void Encoding::pair(LEncodedElt& L, REncodedElt& R, EncodedProduct& result) {
	BOOL OK;

	testOnCurve(L);
	testOnCurve(R);

	// Pairing library doesn't handle g^0 values properly, so check for them
	if (isZero(L) || isZero(R)) {
		//OK = set_immediate(result, DIGIT_ONE, bn.ExtField.elng, PBIGCTX_PASS); 
		OK = Kimmediate(1, result, &bn.ExtField, PBIGCTX_PASS); 
		assert(OK);
	} else {
		// Despite the name, bn_o_ate_proj expects affine coordinates in
		LEncodedElt Laff;
		REncodedElt Raff;

		affinize(L, Laff);
		affinize(R, Raff);
		OK = bn_o_ate_proj(Raff, Laff, &bn, result, PBIGCTX_PASS);
		assert(OK);
		OK = OK && bn_o_ate_finalexp(result, &bn, result, PBIGCTX_PASS);
		assert(OK);
	}
	this->num_pair++;
}

void Encoding::pairAffine(LEncodedEltAff& L, REncodedEltAff& R, EncodedProduct& result) {
	BOOL OK;

	//testOnCurveAffine(L);
	//testOnCurveAffine(R);

	// Pairing library doesn't handle g^0 values properly, so check for them
	if (isZeroAff(L) || isZeroAff(R)) {
		OK = Kimmediate(1, result, &bn.ExtField, PBIGCTX_PASS);
		assert(OK);
	} else {
		OK = bn_o_ate_affine(R, L, ONEPAIR, NONFIXED, &bn, result, PBIGCTX_PASS);
		assert(OK);
		OK = OK && bn_o_ate_finalexp(result, &bn, result, PBIGCTX_PASS);
		assert(OK);
	}
	this->num_pair++;
}

// Checks whether the plaintexts inside the encodings obey:
// L1*R1 - L2*R2 == L3*R3
bool Encoding::mulSubEquals(LEncodedElt& L1, REncodedElt& R1, LEncodedElt& L2, REncodedElt& R2, LEncodedElt& L3, REncodedElt& R3) {
	EncodedProduct pairing1;
	EncodedProduct pairing2;
	EncodedProduct pairing3;

	testOnCurve(L1); testOnCurve(R1);
	testOnCurve(L2); testOnCurve(R2);
	testOnCurve(L3); testOnCurve(R3);

	// Compute the three pairings
	pair(L1, R1, pairing1);
	pair(L2, R2, pairing2);
	pair(L3, R3, pairing3);

#ifdef DEBUG_EC
	// Try computing via affine to see if we have the right inputs at least
	LEncodedEltAff L1a, L2a, L3a;	// Fortunately, projective points are larger than affine, so we have enough room
	REncodedEltAff R1a, R2a, R3a;

	affinize(L1, L1a);
	affinize(L2, L2a);
	affinize(L3, L3a);
	affinize(R1, R1a);
	affinize(R2, R2a);
	affinize(R3, R3a);

	EncodedProduct pairing1a;
	EncodedProduct pairing2a;
	EncodedProduct pairing3a;

	pairAffine(L1a, R1a, pairing1a);
	pairAffine(L2a, R2a, pairing2a);
	pairAffine(L3a, R3a, pairing3a);

	assert(Kequal(pairing1, pairing1a, &bn.ExtField, PBIGCTX_PASS));
	assert(Kequal(pairing2, pairing2a, &bn.ExtField, PBIGCTX_PASS));
	assert(Kequal(pairing3, pairing3a, &bn.ExtField, PBIGCTX_PASS));
#endif 

	// Compute (with some notation abuse) L1*R1 - L2*R2 (in the exponents), i.e., pairing1 <- pairing1 / pairing2
	sub(pairing1, pairing2, pairing1);
	bool result = equals(pairing1, pairing3);
	//BOOL result = ecprojective_equivalent(pairing1, pairing3, &bn.ExtCurve, tmps, PBIGCTX_PASS);
	
#ifdef DEBUG_EC
	sub(pairing1a, pairing2a, pairing1a);
	bool resultAffine = equals(pairing1a, pairing3a); 

	assert(equals(pairing1, pairing1a));
	assert(equals(pairing2, pairing2a));
	assert(equals(pairing3, pairing3a));

	assert((resultAffine != 0 && result != 0) || (resultAffine == 0 && result == 0));
#endif

	return result != 0;  // The != 0 supresses a compiler warning about casting a BOOL (int) to a bool
}


// Checks whether the plaintexts inside the encodings obey:
// L1*R1 == L2*R2
bool Encoding::mulEquals(LEncodedElt& L1, REncodedElt& R1, LEncodedElt& L2, REncodedElt& R2) {
	EncodedProduct pairing1;
	EncodedProduct pairing2;

	testOnCurve(L1); testOnCurve(R1);
	testOnCurve(L2); testOnCurve(R2);

	// Compute the two pairings
	pair(L1, R1, pairing1);
	pair(L2, R2, pairing2);

	BOOL result = Kequal(pairing1, pairing2, &bn.ExtField, PBIGCTX_PASS);
	
	return result != 0;  // The != 0 supresses a compiler warning about casting a BOOL (int) to a bool
}


bool Encoding::equals(LEncodedElt& a, LEncodedElt& b) {
	return ecprojective_equivalent(a, b, &bn.BaseCurve, arithTempSpace, PBIGCTX_PASS) != 0;  // The != 0 supresses a compiler warning about casting a BOOL (int) to a bool
	//return ecaffine_equal(a, b, &bn.BaseCurve, PBIGCTX_PASS) != 0;  // The != 0 supresses a compiler warning about casting a BOOL (int) to a bool
}

bool Encoding::equals(REncodedElt& a, REncodedElt& b) {
	return ecprojective_equivalent(a, b, &bn.TwistCurve, arithTempSpace, PBIGCTX_PASS) != 0;  // The != 0 supresses a compiler warning about casting a BOOL (int) to a bool
	//return ecaffine_equal(a, b, &bn.TwistCurve, PBIGCTX_PASS) != 0;  // The != 0 supresses a compiler warning about casting a BOOL (int) to a bool
}

void Encoding::copy(LEncodedElt& src, LEncodedElt& dst) {
	memcpy(dst, src, sizeof(LEncodedElt));
}

void Encoding::copy(REncodedElt& src, REncodedElt& dst) {
	memcpy(dst, src, sizeof(REncodedElt));
}


#else // DEBUG_ENCODING

Encoding::Encoding() {
	pbigctx = &this->BignumCtx;
	memset(this->pbigctx, 0, sizeof(bigctx_t));

	digit_t prime = 4294967291;
	//digit_t prime = 49999;
	//digit_t prime = 1223;
	//digit_t prime = 101;
	//digit_t prime = 11;

	// Create a small prime field
	p = new mp_modulus_t; 
	BOOL OK = create_modulus(&prime, 1, FROM_LEFT, p, PBIGCTX_PASS);
	assert(OK);

	msExponentField = new field_desc_t; 

	OK = Kinitialize_prime(p, msExponentField, PBIGCTX_PASS);
	assert(OK);

	srcField = new Field(msExponentField);
	Poly::Initialize(srcField);

	zero(Zero);
	memcpy(One, msExponentField->one, sizeof(FieldElt));

	num_exponent_bits = (int) (msExponentField->elng * sizeof(digit_t) * 8);
}

Encoding::~Encoding() {
	delete srcField;
	delete msExponentField;
	uncreate_modulus(p, PBIGCTX_PASS);
	delete p;
	Poly::Cleanup();
}


void Encoding::multiMulOld(multi_exponent_t* multiExp, int len, LEncodedElt& r) {
	FieldElt zero;
	srcField->zero(zero);
	encode(zero, r);

	for (int i = 0; i < len; i++) {
		LEncodedElt tmp, in1, in2;
		memcpy(in1, multiExp[i].pbase, sizeof(LEncodedElt));
		memcpy(in2, multiExp[i].pexponent, sizeof(LEncodedElt));

		mul(in1, in2, tmp);
		add(r, tmp, r);
	}
}

void Encoding::precomputePowersFast(long long numExps, int maxMemGb, bool precompFree) {
	;
}

void Encoding::doubleIt(EncodedElt& a, EncodedElt& b) {
	FieldElt two;

	BOOL OK = Kimmediate(2, two, msExponentField, PBIGCTX_PASS);
	assert(OK);

	mul(a, two, b);
}

void Encoding::print(EncodedElt& elt) {
	this->srcField->print(elt);
}

void Encoding::encode(FieldElt& in, EncodedElt& out) {
	srcField->copy(in, out);
}

void Encoding::zero(EncodedElt& a) { 
	FieldElt zero;
	memset(zero, 0, sizeof(FieldElt));

	encode(zero, a);
}

void Encoding::one(EncodedElt& a) { 
	BOOL OK = Kimmediate(1, a, msExponentField, PBIGCTX_PASS);
	assert(OK);
}

// Multiplies the value encoded in g by the constant c.  Result goes in r.
// For pairings, we compute r <- g^c
void Encoding::mul(EncodedElt& g, FieldElt& c, EncodedElt& r) {	
	srcField->mul(g, c, r);
}


// Adds two encoded elements.  Result goes in r.
void Encoding::add(EncodedElt& a, EncodedElt& b, EncodedElt& r) {
	srcField->add(a, b, r);
}

// Subtracts two encoded elements.  Result goes in r.
void Encoding::sub(EncodedElt& a, EncodedElt& b, EncodedElt& r) {
	srcField->sub(a, b, r);
}


// Checks whether the plaintexts inside the encodings obey:
// L1*R1 - L2*R2 == L3*R3
bool Encoding::mulSubEquals(LEncodedElt& L1, REncodedElt& R1, LEncodedElt& L2, REncodedElt& R2, LEncodedElt& L3, REncodedElt& R3) {
	FieldElt tmp1, tmp2, tmp3;

	mul(L1, R1, tmp1);
	mul(L2, R2, tmp2);
	mul(L3, R3, tmp3);

	sub(tmp1, tmp2, tmp1);

	return srcField->equal(tmp1, tmp3);
}

bool Encoding::mulEquals(LEncodedElt& L1, REncodedElt& R1, LEncodedElt& L2, REncodedElt& R2) {
	FieldElt tmp1, tmp2;
	mul(L1, R1, tmp1);
	mul(L2, R2, tmp2);

	return srcField->equal(tmp1, tmp2);
}

bool Encoding::equals(EncodedElt& a, EncodedElt& b) {
	return srcField->equal(a, b);
}

void Encoding::copy(LEncodedElt& src, LEncodedElt& dst) {
	memcpy(dst, src, sizeof(LEncodedElt));
}


void Encoding::compress(LEncodedElt& in, LCondensedEncElt& out) {
	copy(in, out);
}

void Encoding::compressMany(LEncodedElt* in, LCondensedEncElt* out, int count, bool overWriteOk) {
	for (int i = 0; i < count; i++) {
		compress(in[i], out[i]);
	}
}

void Encoding::decompress(LCondensedEncElt& in, LEncodedElt& out) {
	copy(in, out);
}

void Encoding::decompressMany(LCondensedEncElt* in, LEncodedElt* out, int count) {
	for (int i = 0; i < count; i++) {
		decompress(in[i], out[i]);
	}
}

#endif // DEBUG_ENCODING

#ifndef OLD_MULTI_EXP

// Let VS know we want both versions instantiated
template void Encoding::precomputeMultiPowers(MultiExpPrecompTable<LEncodedElt>& table, LEncodedElt* bases, int len, int numUses, int maxMemGb, bool precompFree);
template void Encoding::precomputeMultiPowers(MultiExpPrecompTable<REncodedElt>& table, REncodedElt* bases, int len, int numUses, int maxMemGb, bool precompFree);

// Wrapper for the version below, if you already have an array, this one will build the indirection layer for you
template <typename EncodedEltType>
void Encoding::precomputeMultiPowers(MultiExpPrecompTable<EncodedEltType>& table, EncodedEltType* bases, int len, int numUses, int maxMemGb, bool precompFree) {
		EncodedEltType** basePtrs = new EncodedEltType*[len];		
		for (int i = 0; i < len; i++) {
			basePtrs[i] = &bases[i];
		}

		precomputeMultiPowers(table, basePtrs, len, numUses, maxMemGb, precompFree);
		
		delete [] basePtrs;
}


template void Encoding::precomputeMultiPowers(MultiExpPrecompTable<LEncodedElt>& table, LEncodedElt** bases, int len, int numUses, int maxMemGb, bool precompFree);
template void Encoding::precomputeMultiPowers(MultiExpPrecompTable<REncodedElt>& table, REncodedElt** bases, int len, int numUses, int maxMemGb, bool precompFree);

// TODO: Reduce memory footprint by not storing g^0 in every table!
template <typename EncodedEltType>
void Encoding::precomputeMultiPowers(MultiExpPrecompTable<EncodedEltType>& table, EncodedEltType** bases, int len, int numUses, int maxMemGb, bool precompFree) {
	double maxMem = (double) maxMemGb * (((long long)1)<<30);
	int numTerms = 2;		// Can't handle more than 2
	int numBits = 1;

	if (len == 0) { table.precompTables = NULL; return; }

	long long numTables = (int)ceil(len / (float)numTerms);
	long long minCost = INT64_MAX;
	while (true) {
		long long perTableSize = 1 << (numBits * numTerms);		// 2^{numBits*numTerms}
		long long totalSizeInB = perTableSize * numTables * sizeof(EncodedEltType);

		long long preCompDbls = (long long) (numTables * ((1 << numBits) / 2.0 + (1 << numBits) / 2.0));		// Slight under-counting, since it doesn't include the (j=k and j/2=k/2=0 mod 2) case
		long long preCompMults = numTables * (perTableSize - numTerms - 1) - preCompDbls;	// Worst-case: 1 mult per table entry.  Each base (plus 0), is simply a copy.  We replace some mults with doublings
		
		long long onlineMults = numUses * (long long)(num_exponent_bits * len / (float)(numTerms * numBits) - 1);	// (DW-1)
		long long onlineDbls = numUses * (long long)(num_exponent_bits / (float) numBits);	// (W-1)

		long long preCost = preCompMults * 3 + preCompDbls;		// Mults are ~3x dbls
		long long onlineCost = onlineMults * 3 + onlineDbls;
		long long totalCost = preCost + onlineCost;

		if (totalSizeInB > maxMem || (!precompFree && totalCost > minCost)) {  // We're always constrained by memory.  If precomp isn't free, we're also constrained to avoid spending too much time on precomp
			// Too big!  Back up.
			numBits--;
			break;
		} else {
			minCost = totalCost;
			numBits++;
		}
	}

	table.numTerms = numTerms;
	table.numBits = numBits;
	table.len = len;
	table.numTables = (int)ceil(len / (float)table.numTerms);
	int tableSize = 1 << (numBits * numTerms);		// 2^{numBits*numTerms}
	table.sizeInMB = (long long) (tableSize * table.numTables * sizeof(EncodedEltType) / (1024.0 * 1024.0));
	printf("Precomputing multi-exp tables: %d bits from %d terms over %d bases. Size=%dMB\n", numBits, numTerms, len, table.sizeInMB);
	
	table.precompTables = new EncodedEltType*[table.numTables];
	for (int i = 0; i < table.numTables; i++) {
		table.precompTables[i] = new EncodedEltType[tableSize];
		int baseIndex = numTerms * i;
		bool oddNumBases = false;

		if (baseIndex >= len - 1) {	// Deal with the case where there are an odd number of bases
			baseIndex--;
			oddNumBases = true;
		}

		zero(table.precompTables[i][0]);  // g^0		
		// Compute powers of the first base
		for (int j = 1; j < (1 << numBits); j++) {
			if (j == 1) {
				copy(*bases[baseIndex + 1], table.precompTables[i][j]);	// g_2^1
			} else if (j % 2 == 0) {	// Double instead of adding, since doubling is cheaper
				doubleIt(table.precompTables[i][j/2], table.precompTables[i][j]);
		  }	else {				
				add(*bases[baseIndex + 1], table.precompTables[i][j-1], table.precompTables[i][j]); // g_2^j
			}
		}

		if (oddNumBases) { // If there are an odd # of bases, then we're done
			break;
		}

		// Multiply by powers of the second base
		for (int j = 1; j < (1 << numBits); j++) {
			int oldTableIndex = (j-1) * (1 << numBits);
			int tableIndex = j * (1 << numBits);
			if (j == 1) {
				copy(*bases[baseIndex], table.precompTables[i][tableIndex]);	// g_1^1
			} else if (j % 2 == 0) {	// Double instead of adding, since doubling is cheaper
				oldTableIndex = (j/2) * (1 << numBits);
				doubleIt(table.precompTables[i][oldTableIndex], table.precompTables[i][tableIndex]);
			} else {
				add(*bases[baseIndex], table.precompTables[i][oldTableIndex], table.precompTables[i][tableIndex]); // g_1^j
			}

			for (int k = 1; k < (1 << numBits); k++) {
				if (j == k && j % 2 == 0 && k % 2 == 0) {	// We can squeeze out an additional doubling as g_1^j * g_2^k = (g_1^j/2 * g_2^k/2)^2
					int halfIndex = (j/2) * (1 << numBits) + k/2;
					doubleIt(table.precompTables[i][halfIndex], table.precompTables[i][tableIndex + k]);
				} else {
					add(table.precompTables[i][tableIndex], table.precompTables[i][k], table.precompTables[i][tableIndex + k]);		// g_1^j * g_2^k
				}
			}
		}
	}
	/*
	LEncodedElt** precompTable = new LEncodedElt*[numTables];
	for (int i = 0; i < numTables; i++) {
		precompTable[i] = new LEncodedElt[tableSize];
		int baseIndex = numTerms * i;

		copy(lG[0][0], precompTable[i][0]);
		for (int j = 1; j < tableSize; j++) {
			// Compute powers of the bases, i.e., g_1^2^k, g_2^2^k, ...
			for (int k = 0; k < numTerms; k++) {
				int step = 1<<(k*numBits);
				copy(bases[baseIndex + numTerms - 1 - k], precompTable[i][step]);
				for (int m = 2; m < (1 << numBits); m++) {
					int prevIndex = step * (m-1);
					int curIndex = step * m;
					doubleIt(precompTable[i][prevIndex], precompTable[i][curIndex]);
				}
			}

			// Compute the remaining terms as products of powers
			for (int k = 0; k < numTerms; k++) {

			}
		}
	}
	*/
	/*
	for (int i = 0; i < tableSize; i++) {
		digit_t foo = table.precompTables[0][i][0];
		printf ("%llu, ", foo);
	}
	*/
	
}


#if 0
// Let VS know we want both versions instantiated
template void Encoding::multiMulVert(MultiExpPrecompTable<LEncodedElt>& precompTables,	FieldElt* exp, int len, LEncodedElt& r);
template void Encoding::multiMulVert(MultiExpPrecompTable<REncodedElt>& precompTables,	FieldElt* exp, int len, REncodedElt& r);

// Computes r <- g_{a_i}^{b_i} for 0 <= i < len
// Computes "vertically", in the sense that it takes each set of numTerm bases
// and does all of the necessary multiplications and doublings.
// It only combines these values at the very end
// Note, this is strictly worse than the multiMul which implements a "horizontal" version.  This is here only for perf comparisons.
template <typename EncodedEltType, typename TableType>
void Encoding::multiMulVert(MultiExpPrecompTable<EncodedEltType>& precompTables,	FieldElt* exp, int len, EncodedEltType& r) {
	// Uses the precomputed tables to step through each set of bases and each set of exponents faster
	int numTables = (len - 1) / numTerms + 1;	  // = ceil (len/numTerms)

	zero(r);  // g^0
	for (int i = 0; i < numTables; i++) {	// For each grouping of numTerm terms
		int expIndex = i * numTerms;
		EncodedEltType tmp;
		zero(tmp);  // g^0
		int chunkSize = (num_exponent_bits-1 - 1) / numBits + 1; // ceil((num_exponent_bits-1) / numBits)
		for (int j = 0; j < chunkSize; j++) {	// For each chunk of exponent bits
			// Build an index into the table, based on the values of the exponents
			int tableIndex = 0;	 

			int bitIndex = max((num_exponent_bits-1) - ((j+1) * numBits), 0);
			int prevBitIndex = (num_exponent_bits-1) - ((j) * numBits);
			int numBitsToGet = min(numBits, prevBitIndex);

			for (int k = 0; k < numBitsToGet; k++) {
				doubleIt(tmp, tmp);
			}
			
			for (int k = expIndex; k < expIndex + numTerms && k < len; k++) {	// For each exponent in this group
				// Grab the appropriate bits of the current exponent				
				int bits = srcField->getBits(exp[k], bitIndex, numBitsToGet);

				tableIndex <<= numBits;	// Make room for the new contribution (we still use numBits to shift, since that's how the table is built)
				tableIndex |= bits;
			}
			//printf("Table index = %d\n", tableIndex);
			add(tmp, precompTables[i][tableIndex], tmp);			
		}
		add(r, tmp, r);
	}
}
*/

#endif

// Let VS know we want both versions instantiated
template void Encoding::multiMul(MultiExpPrecompTable<LEncodedElt>& precompTables, FieldElt* exp, int len, LEncodedElt& r);
template void Encoding::multiMul(MultiExpPrecompTable<REncodedElt>& precompTables, FieldElt* exp, int len, REncodedElt& r);

// Wrapper for the version below, if you already have an array, this one will build the indirection layer for you
template <typename EncodedEltType>
void Encoding::multiMul(MultiExpPrecompTable<EncodedEltType>& precompTables,	FieldElt* exp, int len, EncodedEltType& r) {
		FieldElt** exponents = new FieldElt*[len];		
		for (int i = 0; i < len; i++) {
			exponents[i] = &exp[i];
		}
		multiMul(precompTables, exponents, len, r);
		delete [] exponents;
}

// Let VS know we want both versions instantiated
template void Encoding::multiMul(MultiExpPrecompTable<LEncodedElt>& precompTables, FieldElt** exp, int len, LEncodedElt& r);
template void Encoding::multiMul(MultiExpPrecompTable<REncodedElt>& precompTables, FieldElt** exp, int len, REncodedElt& r);

// Computes r <- g_{a_i}^{b_i} for 0 <= i < len
// Computes "horizontally" in the sense that for each chunk of the exponent space,
// we combine the appropriate table entries for all exponent-base sets,
// and then we use doubling to "move" the values into place.
// This saves a factor of chunkSize doublings compared to the vertical approach
template <typename EncodedEltType>
void Encoding::multiMul(MultiExpPrecompTable<EncodedEltType>& precompTables,	FieldElt** exp, int len, EncodedEltType& r) {
	// Uses the precomputed tables to step through each set of bases and each set of exponents faster
	EncodedEltType tmp;
	int chunkSize = (num_exponent_bits-1 - 1) / precompTables.numBits + 1; // ceil((num_exponent_bits-1) / numBits)
	zero(r);  // g^0
	bool rIsZero = true;		// As long as r remain zero, we can skip many ops
	bool tmpIsZero = true;

	for (int j = 0; j < chunkSize; j++) {	// For each chunk of exponent bits
		zero(tmp);  // g^0
		tmpIsZero = true;

		int bitIndex = max((num_exponent_bits-1) - ((j+1) * precompTables.numBits), 0);
		int prevBitIndex = (num_exponent_bits-1) - ((j) * precompTables.numBits);
		int numBitsToGet = min(precompTables.numBits, prevBitIndex);

		if (!rIsZero) {
			for (int k = 0; k < numBitsToGet; k++) {
				doubleIt(r, r);
			}
		}

		for (int i = 0; i < precompTables.numTables && i*precompTables.numTerms < len ; i++) {	// For each grouping of numTerm terms
			int expIndex = i * precompTables.numTerms;

				// Build an index into the table, based on the values of the exponents
			int tableIndex = 0;	 			
			for (int k = expIndex; k < expIndex + precompTables.numTerms && k < len; k++) {	// For each exponent in this group
				// Grab the appropriate bits of the current exponent				
				int bits = srcField->getBits(*exp[k], bitIndex, numBitsToGet);

				tableIndex <<= precompTables.numBits;	// Make room for the new contribution (we still use numBits to shift, since that's how the table is built)
				tableIndex |= bits;
			}

			// How many bases are in this chunk?
			int leftOver = 0;
			if (len <= (precompTables.len / precompTables.numTerms) * precompTables.numTerms) {	// Len is not in the (possible) leftover chunk at the end
				leftOver = precompTables.numTerms - (len - expIndex);
			} else {	// Can only have as many leftovers as there were assigned the extra chunk at the end to begin with
				leftOver = (precompTables.len % precompTables.numTerms) - (len - expIndex);
			}

			// Adjust tableIndex to account for missing terms in this chunk
			for (int k = 0; k < leftOver; k++) {
				tableIndex <<= precompTables.numBits;
			}

			//printf("Table index = %d\n", tableIndex);
			if (tableIndex != 0) {	// No point adding a 0 term
				if (tmpIsZero) {	// Copy instead of adding 0
					copy(precompTables.precompTables[i][tableIndex], tmp);
					tmpIsZero = false;
				} else {
					add(tmp, precompTables.precompTables[i][tableIndex], tmp);			
				}
				rIsZero = false;
			} 
		}

		if (!tmpIsZero) {  // No point adding a 0 term
			if (rIsZero) { // Copy instead of adding 0
				copy (tmp, r);
			} else {
				add(r, tmp, r);
			}
		}		
	}
}

// Let VS know we want both versions instantiated
template void Encoding::deleteMultiTables(MultiExpPrecompTable<LEncodedElt>& precompTables);
template void Encoding::deleteMultiTables(MultiExpPrecompTable<REncodedElt>& precompTables);

template <typename EncodedEltType>
void Encoding::deleteMultiTables(MultiExpPrecompTable<EncodedEltType>& precompTables) {
	for (int i = 0; i < precompTables.numTables; i++) {
		delete [] precompTables.precompTables[i];
	}

	delete [] precompTables.precompTables;
}


#else // OLD_MULTI_EXP

// Let VS know we want both versions instantiated
template void Encoding::precomputeMultiPowers(MultiExpPrecompTable<LEncodedElt>& table, LEncodedElt* bases, int len, int numTerms, int numBits);
template void Encoding::precomputeMultiPowers(MultiExpPrecompTable<REncodedElt>& table, REncodedElt* bases, int len, int numTerms, int numBits);

// Wrapper for the version below, if you already have an array, this one will build the indirection layer for you
template <typename EncodedEltType>
void Encoding::precomputeMultiPowers(MultiExpPrecompTable<EncodedEltType>& table, EncodedEltType* bases, int len, int numTerms, int numBits) {
		EncodedEltType** basePtrs = new EncodedEltType*[len];		
		for (int i = 0; i < len; i++) {
			basePtrs[i] = &bases[i];
		}

		precomputeMultiPowers(table, basePtrs, len, numTerms, numBits);
		
		delete [] basePtrs;
}


template void Encoding::precomputeMultiPowers(MultiExpPrecompTable<LEncodedElt>& table, LEncodedElt** bases, int len, int numTerms, int numBits);
template void Encoding::precomputeMultiPowers(MultiExpPrecompTable<REncodedElt>& table, REncodedElt** bases, int len, int numTerms, int numBits);

template <typename EncodedEltType>
void Encoding::precomputeMultiPowers(MultiExpPrecompTable<EncodedEltType>& table, EncodedEltType** bases, int len, int numTerms, int numBits) {
	table.multiExp = new multi_exponent_t[len];
	for (int i = 0; i < len; i++) {
		table.multiExp[i].pbase = (digit_t*)bases[i];

		table.multiExp[i].lng_bits = sizeof(FieldElt) * 8 -2;
		table.multiExp[i].lng_pexp_alloc =	(int) ceil((double)table.multiExp[i].lng_bits / (sizeof(digit_t) * 8));		
		table.multiExp[i].offset_bits = 0; 
	}
}


// Let VS know we want both versions instantiated
template void Encoding::multiMul(MultiExpPrecompTable<LEncodedElt>& precompTables, FieldElt* exp, int len, LEncodedElt& r);
template void Encoding::multiMul(MultiExpPrecompTable<REncodedElt>& precompTables, FieldElt* exp, int len, REncodedElt& r);

// Wrapper for the version below, if you already have an array, this one will build the indirection layer for you
template <typename EncodedEltType>
void Encoding::multiMul(MultiExpPrecompTable<EncodedEltType>& precompTables,	FieldElt* exp, int len, EncodedEltType& r) {
		FieldElt** exponents = new FieldElt*[len];		
		for (int i = 0; i < len; i++) {
			exponents[i] = &exp[i];
		}
		multiMul(precompTables, exponents, len, r);
		delete [] exponents;
}

// Let VS know we want both versions instantiated
template void Encoding::multiMul(MultiExpPrecompTable<LEncodedElt>& precompTables, FieldElt** exp, int len, LEncodedElt& r);
template void Encoding::multiMul(MultiExpPrecompTable<REncodedElt>& precompTables, FieldElt** exp, int len, REncodedElt& r);

// Computes r <- g_{a_i}^{b_i} for 0 <= i < len
// Computes "horizontally" in the sense that for each chunk of the exponent space,
// we combine the appropriate table entries for all exponent-base sets,
// and then we use doubling to "move" the values into place.
// This saves a factor of chunkSize doublings compared to the vertical approach
template <typename EncodedEltType>
void Encoding::multiMul(MultiExpPrecompTable<EncodedEltType>& precompTables,	FieldElt** exp, int len, EncodedEltType& r) {
	for (int i = 0; i < len; i++) {
		precompTables.multiExp[i].pexponent = (digit_t*)exp[i];
	}

	// Do it!
	this->multiMulOld(precompTables.multiExp, len, r);
}

// Let VS know we want both versions instantiated
template void Encoding::deleteMultiTables(MultiExpPrecompTable<LEncodedElt>& precompTables);
template void Encoding::deleteMultiTables(MultiExpPrecompTable<REncodedElt>& precompTables);

template <typename EncodedEltType>
void Encoding::deleteMultiTables(MultiExpPrecompTable<EncodedEltType>& precompTables) {
	delete [] precompTables.multiExp;
}

#endif // OLD_MULTI_EXP


void Encoding::resetCount() {
	numMults = 0;
	numSquares = 0;
}

void Encoding::printCount() {
	printf("(Mults,Squares):\t%d\t%d\n", numMults, numSquares);
}


