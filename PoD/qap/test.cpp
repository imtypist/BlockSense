/*
 *  Various functionality tests, microbenchmarks, and QAP-specific tests
 */

#pragma warning( disable : 4005 )	// Ignore warning about intsafe.h macro redefinitions
#define WIN32_LEAN_AND_MEAN  // Prevent conflicts in Window.h and WinSock.h

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <boost/program_options.hpp>
#include "Types.h"
#include "timer.h"
#include "QAP.h"
#include "CircuitMatrixMultiply.h"
#include "CircuitMul2.h"
#include "Encoding.h"
#include "Wire.h"
#include "wire-io.h"
#include "FileArchiver.h"
#include "Network.h"

#include <iostream>
#include <sstream>
#include <fstream>

using namespace std;
using namespace boost::program_options;

extern void runPaillierTests(variables_map& config);

//#ifdef __cplusplus
//extern "C" {
//#endif
//extern void printCount();
//extern void resetCount();
//#ifdef __cplusplus
//}
//#endif

#pragma warning( disable : 4800 )	// Ignore warning about using .count() as bool

// Used in everything but the perf tests
#define N 3
#define NUM_TESTS 100
#define MAX_DEGREE 50


/****************************************************************************
											Polynomial Library Tests
 ****************************************************************************/

// Polynomial to check, expected degree, name of the test, 
// list of coefficients for the poly, starting at x^0
// Hacktastic, but it works
#define CHECK(val, degree, testname, ...) { \
	int c[] = {__VA_ARGS__};	\
	Poly poly(field, degree, c); \
	if (!poly.equals(val)) { printf("Failed: %s\n", testname); } \
}

// Generates a random polynomial of specified degree, or random degree if degree=-1
void polyRand(Field* field, Poly& r, int degree = -1) {
	if (degree == -1) {
		r.setDegree((int)((rand() / (double) RAND_MAX) * MAX_DEGREE));
	} else {
		r.setDegree(degree);
	}

	for (int i = 0; i < r.getDegree() + 1; i++) {
		field->assignRandomElt(&r.coefficients[i]);
	}

	if (rand() % 2 == 0) {
		// Make it monic, since there are some odd corner cases there
		field->one(r.coefficients[r.getDegree()]);
	}
}

void randomPolyMulTest() {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();

	Poly p1(field), p2(field), resultFast(field), resultSlow(field);

	for (int trial = 0; trial < NUM_TESTS; trial++) {
		polyRand(field, p1);
		polyRand(field, p2);		

		Poly::mul(p1, p2, resultFast);
		Poly::mulSlow(p1, p2, resultSlow);

		assert(resultFast.equals(resultSlow));
	}

	delete encoding;
}

void randomPolyAddSubTest() {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();

	Poly p1(field), p2(field), p3(field), result(field);

	for (int trial = 0; trial < NUM_TESTS; trial++) {
		polyRand(field, p1);
		polyRand(field, p2);		
		polyRand(field, p3);

		// Test reversability: p1 - p2 - p3 + p2 + p3 == p1
		Poly::sub(p1, p2, result);
		Poly::sub(result, p3, result);
		Poly::add(result, p2, result);
		Poly::add(result, p3, result);

		assert(result.equals(p1));
	}

	delete encoding;
}

void randomPolyAddMulTest() {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();

	Poly p1(field), p2(field), p3(field), result1(field), result2(field), temp(field);

	for (int trial = 0; trial < NUM_TESTS; trial++) {
		polyRand(field, p1);
		polyRand(field, p2);		
		polyRand(field, p3);

		// Test associativity: p1 * (p2 + p3) == p1 * p2 + p1 * p3
		Poly::add(p2, p3, result1);
		Poly::mul(p1, result1, result1);
		
		Poly::mul(p1, p2, temp);
		Poly::mul(p1, p3, result2);
		Poly::add(result2, temp, result2);

		assert(result1.equals(result2));
	}

	delete encoding;
}

void randomPolySubMulTest() {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();

	Poly p1(field), p2(field), p3(field), result1(field), result2(field), temp(field);

	for (int trial = 0; trial < NUM_TESTS; trial++) {
		polyRand(field, p1);
		polyRand(field, p2);		
		polyRand(field, p3);

		// Test associativity: p1 * (p2 - p3) == p1 * p2 - p1 * p3
		Poly::sub(p2, p3, result1);
		Poly::mul(p1, result1, result1);
		
		Poly::mul(p1, p2, temp);
		Poly::mul(p1, p3, result2);
		Poly::sub(temp, result2, result2);

		assert(result1.equals(result2));
	}

	delete encoding;
}

void randomPolyModTest() {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();

	Poly f(field), m(field), q(field), r(field), result(field);

	for (int trial = 0; trial < NUM_TESTS; trial++) {
		int modDegree = (int)((rand() / (double) RAND_MAX) * MAX_DEGREE) + 1;	// +1, b/c we need mod to be non-constant

		polyRand(field, f);
		polyRand(field, m, modDegree);		

		// Ensure m is monic
		field->one(m.coefficients[m.getDegree()]);

		Poly::mod(f, m, q, r);
		
		Poly::mul(q, m, result);
		Poly::add(result, r, result);

		assert(result.equals(f));
	}

	delete encoding;
}

void randomPolyEvalInterpTest(int degree = -1, int numTests=100) {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();

	printf("Interpolating polynomials with degree %d\n", degree);

	Poly f(field);
	for (int trial = 0; trial < numTests; trial++) {
		// Pick a random function
		polyRand(field, f, degree);

		// Pick some random points to evaluate at
		int numPts = f.getDegree() + 1;
		FieldElt* x = new FieldElt[numPts];
		FieldElt* y = new FieldElt[numPts];

		for (int i = 0; i < numPts; i++) {
			field->assignRandomElt(&x[i]);
			// Make sure this value is unique, since we'll be using it for interpolation
			bool unique;
			do {
				unique = true;
				for (int j = 0; j < i; j++) {
					if (field->equal(x[i], x[j])) {
						unique = false;
						field->assignRandomElt(&x[i]);
						break;
					}
				}
			} while (!unique);
		}

		Poly::multiEval(f, x, numPts, y);
		Poly* fprime = Poly::interpolate(field, x, y, numPts);
		
		assert(f.equals(*fprime));
		
		delete [] x;
		delete [] y;
		delete fprime;
	}

	delete encoding;
}

void runPolyCorrectnessTest() {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();

	// Most of these assume the specific prime chosen for encoding debugging
	// They also use implicit array initialization, which doesn't work for large fields
#if defined DEBUG_ENCODING && 0	
	int p1coeff[] = {9, 6, 1};  // p1 = x^2 + 6x + 9
	Poly p1(field, sizeof(p1coeff)/sizeof(int) - 1, p1coeff);

	int p2coeff[] = {4, 4, 1};  // p2 = x^2 + 4x + 4
	Poly p2(field, sizeof(p2coeff)/sizeof(int) - 1, p2coeff);

	Poly p3(field);

	field->polyAdd(p1, p2, p3);
	CHECK(p3, 2, "Add", 13, 10, 2);
	//printf("Should be: 13, 10, 2 mod p\n");
	//p3.print(); printf("\n");

	field->polySub(p1, p2, p3);
	CHECK(p3, 2, "sub", 5, 2, 0);
		
	field->polyMul(p1, p2, p3);
	CHECK(p3, 4, "mul", 36, 60, 37, 10, 1);

	Poly pM(field);
	field->polyMulSlow(p1, p2, pM);
	if (!pM.equals(p3)) { printf("Failed multiplication1 test\n"); }

	int p4coeff[] = {6, 5, 4, 3, 2, 1};
	int p5coeff[] = {9, 8, 7, 6, 5, 1};
	Poly p4(field, sizeof(p4coeff)/sizeof(int) - 1, p4coeff);
	Poly p5(field, sizeof(p5coeff)/sizeof(int) - 1, p5coeff);

	field->polyMul(p4, p5, p3);
	CHECK(p3, 10, "big mul", 54, 93, 118, 130, 130, 101, 65, 38, 19, 7, 1);

	field->polyMulSlow(p4, p5, pM);
	if (!pM.equals(p3)) { printf("Failed multiplication2 test\n"); }
#endif // DEBUG_ENCODING

	randomPolyMulTest();
	randomPolyAddSubTest();
	randomPolyAddMulTest();
	randomPolySubMulTest();

#if defined DEBUG_ENCODING && 0
	field->firstDerivative(p1);	// Should be 6 2
	CHECK(p1, 1, "p1'", 6, 2);
	field->firstDerivative(p2);	// Should be 4 2
	CHECK(p2, 1, "p2'", 4, 2);
	field->firstDerivative(p3); // Should be {93, 236, 390, 520, 505, 390, 266, 152, 63, 10}
	CHECK(p3, 9, "p3'", 93, 236, 390, 520, 505, 390, 266, 152, 63, 10);
	field->firstDerivative(p4);	// Should be {5, 8, 9, 8, 5}
	CHECK(p4, 4, "p4'", 5, 8, 9, 8, 5);
	field->firstDerivative(p5); // Should be {8, 14, 18, 20, 5}
	CHECK(p5, 4, "p5'", 8, 14, 18, 20, 5);

	field->polyReverse(p1); // Should be 2 6
	CHECK(p1, 1, "rev1", 2, 6);
	field->polyReverse(p5);	// Should be 5, 20, 18, 14, 8
	CHECK(p5, 4, "rev5", 5, 20, 18, 14, 8); 
	field->polyReverse(p5, p4); // p4 should be {8, 14, 18, 20, 5}
	field->polyReverse(p5); // Should be {8, 14, 18, 20, 5}
	if (!p5.equals(p4)) { printf("Failed reverse\n"); }

	field->polyReduce(p4, 3);	// Should be {8, 14, 18}
	CHECK(p4, 2, "reduce4", 8, 14, 18);
	field->polyReduce(p5, 2);	// Should be {8, 14}
	CHECK(p5, 1, "reduce5", 8, 14);

	int p6coeff[] = {0, 0, 4, 5, 6, 7};
	Poly p6(field, sizeof(p6coeff)/sizeof(int) - 1, p6coeff);
	field->polyDiv(p6, 2, p6);
	CHECK(p6, 3, "div6", 4, 5, 6, 7);
	
	Poly p8(field);
	int p7coeff[] = {1, 2};
	Poly p7(field, sizeof(p7coeff)/sizeof(int) - 1, p7coeff);
	field->polyInvert(p7, 3, p8);
	CHECK(p8, 2, "invert8", 1, 1221, 4);	 // Assuming p = 1223

	int p9coeff[] = {1, 3, 4};
	Poly p9(field, sizeof(p9coeff)/sizeof(int) - 1, p9coeff);
	field->polyInvert(p9, 3, p8);
	CHECK(p8, 2, "invert9", 1, 1220, 5);	 // Assuming p = 1223

	int p10coeff[] = {1, 3, 4};
	Poly p10(field, sizeof(p10coeff)/sizeof(int) - 1, p10coeff);
	field->polyInvert(p10, 4, p8);
	CHECK(p8, 3, "invert10", 1, 1220, 5, 1220);	 // Assuming p = 1223

	int p11coeff[] = {1, 3, 4};
	Poly p11(field, sizeof(p11coeff)/sizeof(int) - 1, p11coeff);
	int p12coeff[] = {2, 1};
	Poly p12(field, sizeof(p12coeff)/sizeof(int) - 1, p12coeff);
	Poly q(field), r(field);
	field->polyMod(p11, p12, q, r);
	CHECK(q, 1, "Quotient", 1218, 4); // Assuming p = 1223
	CHECK(r, 0, "Remainder", 11); 
#endif // DEBUG_ENCODING

	randomPolyModTest();

#if defined DEBUG_ENCODING && 0
	int p13coeff[] = {2, 1};
	Poly p13(field, sizeof(p13coeff)/sizeof(int) - 1, p13coeff);	
	FieldElt x1[] = {3, 4, 5, 6};
	FieldElt y1[sizeof(x1)/sizeof(FieldElt)];
	field->multiEval(p13, x1, 4, y1);

	FieldElt answer1[] = {5, 6, 7, 8};
	for (int i = 0; i < sizeof(y1)/sizeof(FieldElt); i++) {
		if (!field->equal(y1[i], answer1[i])) {
			printf("Failed multiEval1 at position %d\n", i);
		}
	}

	int p14coeff[] = {0, 0, 1};
	Poly p14(field, sizeof(p14coeff)/sizeof(int) - 1, p14coeff);	
	FieldElt x2[] = {1, 2, 3, 4, 5, 6, 7};
	FieldElt y2[sizeof(x2)/sizeof(FieldElt)];
	field->multiEval(p14, x2, 7, y2);

	FieldElt answer2[sizeof(x2)/sizeof(FieldElt)] = {1, 4, 9, 16, 25, 36, 49};
	for (int i = 0; i < sizeof(y2)/sizeof(FieldElt); i++) {
		if (!field->equal(y2[i], answer2[i])) {
			printf("Failed multiEval2 at position %d\n", i);
		}
	}

	int p15coeff[] = {4, 2, 1};
	Poly p15(field, sizeof(p15coeff)/sizeof(int) - 1, p15coeff);	
	FieldElt x3[] = {1, 2, 3};
	FieldElt y3[sizeof(x3)/sizeof(FieldElt)];
	field->multiEval(p15, x3, 3, y3);

	FieldElt answer3[sizeof(x3)/sizeof(FieldElt)] = {7, 12, 19};
	for (int i = 0; i < sizeof(y3)/sizeof(FieldElt); i++) {
		if (!field->equal(y3[i], answer3[i])) {
			printf("Failed multiEval3 at position %d\n", i);
		}
	}

	FieldElt x5[] = {1, 2};	
	FieldElt y5[] = {3, 3};		// y = 3
	Poly* p16 = field->interpolate(x5, y5, sizeof(x5) / sizeof(FieldElt));
	CHECK((*p16), 0, "Interpolate16", 3);
	delete p16;

	FieldElt x17[] = {1, 2};	
	FieldElt y17[] = {5, 5};		// y = 5
	Poly* p17 = field->interpolate(x17, y17, sizeof(x17) / sizeof(FieldElt));
	CHECK((*p17), 0, "Interpolate17", 5);
	delete p17;

	FieldElt x18[] = {1, 2};	
	FieldElt y18[] = {5, 7};		// y = 2x + 3
	Poly* p18 = field->interpolate(x18, y18, sizeof(x18) / sizeof(FieldElt));
	CHECK((*p18), 1, "Interpolate18", 3, 2);
	delete p18;

	FieldElt x19[] = {1, 2, 3};	
	FieldElt y19[] = {6, 11, 18};		// y = x^2 + 2x + 3
	Poly* p19 = field->interpolate(x19, y19, sizeof(x19) / sizeof(FieldElt));
	CHECK((*p19), 2, "Interpolate19", 3, 2, 1);
	delete p19;

	FieldElt x20[] = {1, 2, 3, 4};	
	FieldElt y20[] = {12, 25, 54, 105};		// y = x^3 + 2 x^2 + 0x + 9
	Poly* p20 = field->interpolate(x20, y20, sizeof(x20) / sizeof(FieldElt));
	CHECK((*p20), 3, "Interpolate20", 9, 0, 2, 1);
	delete p20;

	FieldElt x21[] = {1, 2, 3, 4, 5};	
	FieldElt y21[] = {15, 57, 189, 489, 1059};		// y = x^4 + 3 x^3 + 2 x^2 + 0x + 9
	Poly* p21 = field->interpolate(x21, y21, sizeof(x21) / sizeof(FieldElt));
	CHECK((*p21), 4, "Interpolate21", 9, 0, 2, 3, 1);
	delete p21;
#endif // DEBUG_ENCODING

	randomPolyEvalInterpTest();

	delete encoding;
}

/****** Test the FFT library ******/
void runFftFunctionalityTest() {
	bigctx_t BignumCtx;
	bigctx_t* pbigctx;

	pbigctx = &BignumCtx;
	memset(pbigctx, 0, sizeof(bigctx_t));

	//digit_t prime = 1223;
	//digit_t prime = 101;
	digit_t prime = 11;

	// Create a small prime field
	mp_modulus_t* p = new mp_modulus_t; 
	BOOL OK = create_modulus(&prime, 1, FROM_LEFT, p, PBIGCTX_PASS);
	assert(OK);
	
	field_desc_t* primeField = new field_desc_t; 
	OK = Kinitialize_prime(p, primeField, PBIGCTX_PASS);
	assert(OK);

	modfft_info_t fftinfo;
	modfft_init(p, &fftinfo, PBIGCTX_PASS);

	digit_t evalPts[10];
	Kimmediate(7, &evalPts[0], primeField, PBIGCTX_PASS);
	Kimmediate(7, &evalPts[1], primeField, PBIGCTX_PASS);
	Kimmediate(7, &evalPts[2], primeField, PBIGCTX_PASS);
	Kimmediate(7, &evalPts[3], primeField, PBIGCTX_PASS);

	int ip = (int)(fftinfo.nprime/2);
	modfft_preverse(evalPts, 2, ip, &fftinfo, PBIGCTX_PASS);

	int numEvalpts = sizeof(evalPts) / sizeof(digit_t);
	digit_t div;
	digit_t* tmp = new digit_t[primeField->ndigtemps_arith];
	Kimmediate(numEvalpts, &div, primeField, PBIGCTX_PASS);
	for (int i = 0; i < numEvalpts; i++) {
		to_modular(&evalPts[i], 1, &evalPts[i], p, PBIGCTX_PASS);
		BOOL OK = Kdiv(&evalPts[i], &div, &evalPts[i], primeField, tmp, PBIGCTX_PASS); 
		assert(OK);
		//from_modular(&evalPts[i], &evalPts[i], primeField->modulo, PBIGCTX_PASS); 
		assert(OK);
	}

	// Try multiplying some polynomials
	FieldElt result[5], scratch1[12], scratch2[12];

	// (x+7)(x+7) = x^2+3x+5 => {5, 3} 
	Kimmediate(7, &evalPts[0], primeField, PBIGCTX_PASS);
	Kimmediate(2, &evalPts[1], primeField, PBIGCTX_PASS);
	modpoly_monic_product(&evalPts[0], 1, &evalPts[1], 1, result[0], scratch1[0], scratch2[0], &fftinfo, PBIGCTX_PASS);

	// (x+3)(x+9) = x^2+1x+5 => {5, 1} 
	Kimmediate(7, &evalPts[0], primeField, PBIGCTX_PASS);
	Kimmediate(2, &evalPts[1], primeField, PBIGCTX_PASS);
	modpoly_monic_product(&evalPts[0], 1, &evalPts[1], 1, result[0], scratch1[0], scratch2[0], &fftinfo, PBIGCTX_PASS);

	// (x^2+2x+7)(x+3) = x^3+5x^2+2x+10 => {10, 2, 5} 
	Kimmediate(7, &evalPts[0], primeField, PBIGCTX_PASS);
	Kimmediate(2, &evalPts[1], primeField, PBIGCTX_PASS);
	Kimmediate(3, &evalPts[2], primeField, PBIGCTX_PASS);
	modpoly_monic_product(&evalPts[0], 2, &evalPts[2], 1, result[0], scratch1[0], scratch2[0], &fftinfo, PBIGCTX_PASS);

	delete [] tmp;
	delete primeField;
	modfft_uninit(&fftinfo, PBIGCTX_PASS);
	uncreate_modulus(p, PBIGCTX_PASS);
	delete p;
}

/****** Two tests of the multi-exponentiation routines ******/
void runMultiExpTest(int NUM = 1) {
	const int MAX_MEM = 5;
	printf("Running multiExp trials %d times\n", NUM);
	Timer* multiTimer = timers.newTimer("Multi", NULL);
	Timer* newMultiTimer = timers.newTimer("NewMulti", NULL);
	Timer* expTimer = timers.newTimer("Exp", NULL);

	Encoding* encoding = new Encoding();	
	Field* field = encoding->getSrcField();
	multi_exponent_t* multExp = new multi_exponent_t[NUM];
	FieldElt* expGen = new FieldElt[NUM];
	FieldElt* expTest = new FieldElt[NUM];
	FieldElt** expTestPtrs = new FieldElt*[NUM];
	LEncodedElt multiResult, newMultiResult; // slowResult
	LEncodedElt* enc = new LEncodedElt[NUM];
	LEncodedElt** encPtrs = new LEncodedElt*[NUM];

	// Generate some initial g_i ^ e_i values
	encoding->precomputePowersFast(NUM, MAX_MEM, false);
	expTimer->start();
	for (int i = 0; i < NUM; i++) {
		field->assignRandomElt(&expGen[i]);		
		encoding->encode(expGen[i], enc[i]);
		encPtrs[i] = &enc[i];

		field->assignRandomElt(&expTest[i]);
		expTestPtrs[i] = &expTest[i];
	}
	expTimer->stop();

	//for (int i = 0; i < NUM; i++) {
	//	for (int j = 1; j <= 3; j++) {
	//		expTest[i][j] = 0;		// Set all but the top digit_t to 0
	//	}
	//}

	//expTest[0][3] = expTest[0][2] = expTest[0][1] = 0;
	//expTest[1][3] = expTest[1][2] = expTest[1][1] = 0;

	encoding->resetCount();
	MultiExpPrecompTable<LEncodedElt> precompTables;
	newMultiTimer->start();
	TIME(encoding->precomputeMultiPowers(precompTables, encPtrs, NUM, 1, MAX_MEM, false), "PreComp", "NewMulti");
	printf("Table counts:\n");
	encoding->printCount();
	encoding->resetCount();
	TIME(encoding->multiMul(precompTables, expTestPtrs, NUM-1, newMultiResult), "Compute", "NewMulti");
	printf("Work counts:\n");
	encoding->printCount();
	newMultiTimer->stop();

	// Setup the multi-exp	
	for (int i = 0; i < NUM; i++) {
		multExp[i].pbase = enc[i];
		multExp[i].pexponent = expTest[i]; 
		multExp[i].lng_bits = sizeof(FieldElt) * 8;
		multExp[i].lng_pexp_alloc =	multExp[i].lng_bits / (sizeof(digit_t) * 8);		
		multExp[i].offset_bits = 0;	
	}
	
	// Do it!
	//resetCount();
	TIME(encoding->multiMulOld(multExp, NUM, multiResult), "MultiMul", "Multi");
	//printf("\nPeter's counts:");
	//printCount();
	
	if (!encoding->equals(multiResult, newMultiResult)) {
		printf("\nFailed new multi test\n\n");
	} else {
		printf("\nPassed new multi test\n\n");
	}

	timers.printStats();

	encoding->deleteMultiTables(precompTables);
	delete [] expGen;
	delete [] expTest;
	delete [] enc;
	delete [] multExp;	
	delete encoding;
}

void runMultiExpTest2() {
	const int NUM = 1;
	const int MAX_MEM = 5;
	Encoding* encoding = new Encoding();
	encoding->precomputePowersFast(NUM, MAX_MEM, false);
	Field* field = encoding->getSrcField();
	Wire* wires = new Wire[NUM];

	multi_exponent_t* multExp = new multi_exponent_t[NUM];
	FieldElt exp1[NUM];
	FieldElt exp2[NUM];
	REncodedElt enc[NUM], slowResult, multiResult;
	
	// Generate some initial g_i ^ e_i values
	for (int i = 0; i < NUM; i++) {
		field->assignRandomElt(&exp1[i]);
		field->assignRandomElt(&exp2[i]);
		field->copy(exp2[i], wires[i].value);
		encoding->encode(exp1[i], enc[i]);
	}

	// Setup the multi-exp	
	for (int i = 0; i < NUM; i++) {
		multExp[i].pbase = enc[i];
		multExp[i].pexponent = wires[i].value; // exp2[i]; 
		multExp[i].lng_bits = sizeof(FieldElt) * 8;
		multExp[i].lng_pexp_alloc =	multExp[i].lng_bits / (sizeof(digit_t) * 8);		
		multExp[i].offset_bits = 0;	
	}

	// Do it!
	encoding->multiMulOld(multExp, NUM, multiResult);

	// Do it the slow way
	REncodedElt tmp;
	encoding->zero(slowResult);
	for (int i = 0; i < NUM; i++) {
		encoding->mul(enc[i], exp2[i], tmp);
		encoding->add(slowResult, tmp, slowResult);
	}

	if (!encoding->equals(slowResult, multiResult)) {
		printf("Failed multi test\n");
	} else {
		printf("Passed multi test\n");
	}

	delete multExp;
	delete encoding;
	delete wires;
}


/****************************************************************************
											Circuit-related Tests
 ****************************************************************************/


/****** Helper functions the initialize circuit inputs ******/

void assignAllOneInputs(Field* field, WireVector& vector) {
	FieldElt* one = field->newOne();
	for (int i = 0; i < vector.size(); i++) {
		field->copy(*one, vector[i]->value);
	}
	delete [] one;
}

void assignOneZeroInputs(Field* field, WireVector& vector) {
	FieldElt* one = field->newOne();
	for (int i = 0; i < vector.size(); i++) {
		if (i % 2 == 0) {
			field->copy(*one, vector[i]->value);
		} else {
			field->zero(vector[i]->value);
		}
	}
	delete [] one;
}

void assignRandom(Field* field, WireVector& vector, int bits = -1) {
	for (int i = 0; i < vector.size(); i++) {
		field->assignRandomElt(&vector[i]->value);		
	}

	if (bits < 0) {
		return;		// Nothing to do
	}

	// Otherwise, truncate the elements to only use bits number of bits
	const int digitSize = sizeof(digit_t) * 8;
	int nonZeroDigitIndex = bits / digitSize;
	int nonZeroIntraDigitIndex = bits % digitSize;
	digit_t mask = ((digit_t)1 << nonZeroIntraDigitIndex) - 1;

	for (int i = 0; i < vector.size(); i++) {
		for (int j = sizeof(FieldElt)/sizeof(digit_t) - 1; j >=0 ; j--) {
			if (j > nonZeroDigitIndex) {
				vector[i]->value[j] = 0;
			} else if (j == nonZeroDigitIndex) {
				vector[i]->value[j] &= mask;
			} else {
				break;
			}
		}
	}		
}

// Set the last element to 1
void assignConstOne(Field* field, WireVector& vector, int numTrueInputs) {
	FieldElt* one = field->newOne();
	field->copy(*one, vector[numTrueInputs-1]->value);	
	delete [] one;
}

bool equal(Field* field, WireVector& wiresA, WireVector& wiresB) {
	if (wiresA.size() != wiresB.size()) { return false; }

	for (int i = 0; i < wiresA.size(); i++) {
		if (!field->equal(wiresA[i]->value, wiresB[i]->value)) {
			return false;
		}
	}
	return true;
}


/****** Make sure different circuit-representations produce the same result ******/

bool testEval(Field* field, CircuitMatrixMultiply& stdMatMulCircuit, CircuitMatrixMultiply& qapMatMulCircuitNadder, CircuitMatrixMultiply& qapMatxMulCircuit2adder) {
	// Make sure the constant 1 input is really set to one
	assignConstOne(field, qapMatMulCircuitNadder.inputs, qapMatMulCircuitNadder.numTrueInputs());
	assignConstOne(field, qapMatxMulCircuit2adder.inputs, qapMatxMulCircuit2adder.numTrueInputs());

	stdMatMulCircuit.eval(false);
	qapMatMulCircuitNadder.eval(false);
	qapMatxMulCircuit2adder.eval(false);

	if (!equal(field, stdMatMulCircuit.outputs, qapMatMulCircuitNadder.outputs) || 
	    !equal(field, qapMatMulCircuitNadder.outputs, qapMatxMulCircuit2adder.outputs)) { 
		printf("Non-optimized evaluation produced different results!\n");
		return false;
	}

	stdMatMulCircuit.eval(true);
	qapMatMulCircuitNadder.eval(true);
	qapMatxMulCircuit2adder.eval(true);

	if (!equal(field, stdMatMulCircuit.outputs, qapMatMulCircuitNadder.outputs) || 
	    !equal(field, qapMatMulCircuitNadder.outputs, qapMatxMulCircuit2adder.outputs)) {
		printf("Optimized evaluation produced different results!\n");
		return false;
	}
	return true;
}


/****** Check that matrix circuits work properly ******/

bool runMatrixEvalTests() {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();

	CircuitMatrixMultiply stdMatMulCircuit(field, N);
	CircuitMatrixMultiply qapMatMulCircuitNadder(field, N, false);
	CircuitMatrixMultiply qapMatxMulCircuit2adder(field, N, true);

	assignAllOneInputs(field, stdMatMulCircuit.inputs);
	assignAllOneInputs(field, qapMatMulCircuitNadder.inputs);
	assignAllOneInputs(field, qapMatxMulCircuit2adder.inputs);
	if(!testEval(field, stdMatMulCircuit, qapMatMulCircuitNadder, qapMatxMulCircuit2adder)) { return false; }
	
	assignOneZeroInputs(field, stdMatMulCircuit.inputs);
	assignOneZeroInputs(field, qapMatMulCircuitNadder.inputs);
	assignOneZeroInputs(field, qapMatxMulCircuit2adder.inputs);
	if(!testEval(field, stdMatMulCircuit, qapMatMulCircuitNadder, qapMatxMulCircuit2adder)) { return false; }

	assignRandom(field, stdMatMulCircuit.inputs);
	for (int i = 0; i < stdMatMulCircuit.inputs.size(); i++) {
		field->copy(stdMatMulCircuit.inputs[i]->value, qapMatMulCircuitNadder.inputs[i]->value);
		field->copy(stdMatMulCircuit.inputs[i]->value, qapMatxMulCircuit2adder.inputs[i]->value);
	}
	if(!testEval(field, stdMatMulCircuit, qapMatMulCircuitNadder, qapMatxMulCircuit2adder)) { return false; }

	delete encoding;
	return true;
}


/****************************************************************************
											QAP Correctness Tests
 ****************************************************************************/

/****** Check to see that bad proofs still fail verification ******/
/* No longer works reliably -- assertions on well formed group elements catch
   mangling too soon.  Need to do mangling within the group to check this properly */
bool mangleAndCheckProof(digit_t& digit, QAP& qap, Keys* keys, Proof* p) {
	bool ret = false;

#if 0
	// Mangle the indicated digit, but remember its previous state
	digit_t tmp;
	tmp = digit;
	digit = ~digit;

	if (qap.verify(keys, p, config)) {
		printf("Proof passed, but should have failed! ********************\n\n");
		assert(0);
		ret = false;
	} else {
		printf("Invalid proof correctly detected\n");
		ret = true;
	}
	
	// Restore the proof to it's previous state
	digit = tmp;
#endif
	return ret;
}

bool checkMangledProofs(QAP& qap, Keys* keys, Proof* p) {	
	/*
	int lIndex = 0;
	int rIndex = 0;
	int fIndex = 0;
	*/
	// Using purely random here results in changing digits such that the field elt leaves the field
	//int lIndex = (int) ((rand() / (double) RAND_MAX) * sizeof(LEncodedElt) / sizeof(digit_t));
	//int rIndex = (int) ((rand() / (double) RAND_MAX) * sizeof(REncodedElt) / sizeof(digit_t));
	//int fIndex = (int) ((rand() / (double) RAND_MAX) * sizeof(FieldElt) / sizeof(digit_t));

	// Try changing all but the last digit (may still cause problems for ECC points, so just do first point)
	int lIndex = (int) ((rand() / (double) RAND_MAX) * (sizeof(LEncodedElt) / sizeof(digit_t)) / 2 - 1);
	int rIndex = (int) ((rand() / (double) RAND_MAX) * (sizeof(REncodedElt) / sizeof(digit_t)) / 2 - 1);
	int fIndex = (int) ((rand() / (double) RAND_MAX) * ((sizeof(FieldElt) / sizeof(digit_t)) - 1)) ;
	int inIndex = (int) ((rand() / (double) RAND_MAX) * qap.circuit->inputs.size());
	int outIndex = (int) ((rand() / (double) RAND_MAX) * qap.circuit->outputs.size());

	// Try mangling various values in the proof and make sure that verification fails
	bool success = true;
	success = success && mangleAndCheckProof(p->V[lIndex], qap, keys, p);
	success = success && mangleAndCheckProof(p->W[rIndex], qap, keys, p);
	success = success && mangleAndCheckProof(p->Y[lIndex], qap, keys, p);
	success = success && mangleAndCheckProof(p->H[lIndex], qap, keys, p);
	success = success && mangleAndCheckProof(p->alphaV[lIndex], qap, keys, p);
	success = success && mangleAndCheckProof(p->alphaW[rIndex], qap, keys, p);
	success = success && mangleAndCheckProof(p->alphaY[lIndex], qap, keys, p);
//	success = success && mangleAndCheckProof(p->alphaH[lIndex], qap, keys, p);

	success = success && mangleAndCheckProof(p->beta[lIndex], qap, keys, p);	

	success = success && mangleAndCheckProof(p->inputs[inIndex][fIndex], qap, keys, p);
	success = success && mangleAndCheckProof(p->outputs[outIndex][fIndex], qap, keys, p);

	return success;
}

/****** Various basic QAP tests with very simple circuits ******/
/*
  When using DEBUG_ENCODING and DEBUG_RANDOM:
	Creates the following circuit:

	1   2   3   4
	 \ /     \ /
		*       *       
		5       6

	Assigns input values: 1 = 1, 2 = 2, 3 = 3, 4 = 4
	Which results in output values: 5 = 2, 6 = 12

	Assigns root values r_5 = 5, r_6 = 6

	L_5(x) = 6 - x
	L_6(x) = x - 5

	Thus, we have the following polynomials:

	v(x) = 2x - 9
	w(x) = 2x - 8
	y(x) = 10x -48

	t(x) = (x-5)(x-6) = x^2-11x+30

	v(x)w(x) - y(x) = 4x^2-44x+120

	so 

	h(x) = 4

	Assigns s = 7, so:

	E(s^0) = 1
	E(s^1) = 7
	E(s^2) = 49

	v(s) = 5
	w(s) = 6
	y(s) = 22

	v(s)*w(s)-y(s) = 8

	h(s) = 4
	t(s) = 2

	h(s)*t(s) = 8
*/

bool runSimpleTests(variables_map& config) {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	CircuitMul2 circuit(field);
	
	// Test both forms of evaluation on 0x1
	assignOneZeroInputs(field, circuit.inputs);

	FieldElt zero;
	field->zero(zero);
	if (!field->equal(circuit.outputs[0]->value, zero)) {
		printf("Unoptimized eval failed.  Should be 0.\n");
		return false;
	}

	circuit.eval(true);
	if (!field->equal(circuit.outputs[0]->value, zero)) {
		printf("Optimized eval failed.  Should be 0.\n");
		return false;
	}	

	// Test both forms of evaluation on 1x1
	assignAllOneInputs(field, circuit.inputs);
	circuit.eval(false);

	FieldElt one;
	field->one(one);
	if (!field->equal(circuit.outputs[0]->value, one)) {
		printf("Unoptimized eval failed.  Should be 1.\n");
		return false;
	}

	circuit.eval(true);
	if (!field->equal(circuit.outputs[0]->value, one)) {
		printf("Optimized eval failed.  Should be 1.\n");
		return false;
	}

	// Try using random inputs
	assignRandom(field, circuit.inputs);
	circuit.eval(false);

	// Try constructing a QAP
	QAP qap(&circuit, encoding, config.count("pcache") > 0);	

	Keys* keys = qap.genKeys(config);
	Proof* p = qap.doWork(keys->pk, config);

	bool ret = qap.verify(keys, p, config);

	if (ret) {
		printf("Simple proof checks out\n\n");
	} else {
		printf("Simple proof fails ********************\n\n");
		return false;
	}

	
#if 0	
	// We now have too many checks in place (e.g., for a point being on a curve)
	// that this no longer works, at least not until we add proper exception throwing/catching

	// Make sure the proof fails if we alter it
	bool ret = false;
#ifdef DEBUG_ENCODING
	printf("Not checking mangled proofs, since current mangling technique doesn't work on small fields.  Should change mangling to select a random field element\n");
#else // !DEBUG_ENCODING
	ret = checkMangledProofs(qap, keys, p);
#endif // DEBUG_ENCODING

	// Make sure it still passes
	if (qap.verify(keys, p)) {
		printf("Simple proof checks out again\n\n");
		ret = true;
	} else {
		printf("Simple proof fails the second time around ********************\n\n");
		ret = false;
	}
#endif

	delete encoding;
	delete keys;
	delete p;

	return ret;
}


/****** Helper function for Matrix QAP correctness tests ******/

bool runMatrixProofTests(CircuitMatrixMultiply& circuit, Encoding* encoding, const char* testName, variables_map& config) {
	Field* field = encoding->getSrcField();
	assignRandom(field, circuit.inputs);
	//assignAllOneInputs(field, circuit.inputs);
	assignConstOne(field, circuit.inputs, circuit.numTrueInputs()); // Make sure the final input is a 1
	circuit.eval(true);

	// Try constructing a QAP
	QAP qap(&circuit, encoding, config.count("pcache") > 0);	
	Keys* keys = qap.genKeys(config);
	Proof* p = qap.doWork(keys->pk, config);

	if (qap.verify(keys, p, config)) {
		printf("Proof for %s checks out\n\n", testName);
	} else {
		printf("Proof for %s fails ********************\n\n", testName);
		return false;
	}

	// Make sure the proof fails if we alter it
	bool ret = checkMangledProofs(qap, keys, p);

	// Make sure it still passes
	if (qap.verify(keys, p, config)) {
		printf("Simple proof checks out again\n\n");
		ret = true;
	} else {
		printf("Simple proof fails the second time around ********************\n\n");
		ret = false;
	}

	delete keys;
	delete p;

	return ret;
}

/****** Battery of tests to verify that correct matrix QAP proofs pass and bad ones fail ******/
bool runQAPCorrectnessTests(variables_map& config) {
	// Create some circuits to test
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	CircuitMatrixMultiply qapMatMulCircuitNadder(field, N, false);
	CircuitMatrixMultiply qapMatxMulCircuit2adder(field, N, true);

	bool success = true;
	for (int i = 0; i < NUM_TESTS; i++) {
		success = success && runSimpleTests(config);
		success = success && runMatrixEvalTests();	
		success = success && runMatrixProofTests(qapMatMulCircuitNadder,  encoding, "MatMul - Nadder", config);	
		success = success && runMatrixProofTests(qapMatxMulCircuit2adder, encoding, "MatMul - 2adder", config);	
	}

	if (!success) {
		printf("\nOne of the tests failed! ***************\n");
	} else {
		printf("\nAll tests passed!\n");
	}

	delete encoding;

	return success;
}


/****************************************************************************
											Encoding Correctness Tests
 ****************************************************************************/
#ifndef DEBUG_ENCODING
// This test passes!
void runEncodingTest() {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	
	// Pick exponents
	FieldElt exp1, exp2;
	field->assignRandomElt(&exp1);
	field->assignRandomElt(&exp2);

	// Apply to the generator.  Result should be in affine coordinates
	LEncodedElt encodedEltAff1, encodedEltProj1, encodedEltAff2, encodedEltProj2, result;
	BOOL okay = ecaffine_exponentiation_via_multi_projective(encoding->bn.BaseCurve.generator, exp1, encoding->bn.pbits, encoding->bn.BaseCurve.lnggorder, encodedEltAff1, &(encoding->bn.BaseCurve), &encoding->BignumCtx);
	assert(okay);
	okay = ecaffine_exponentiation_via_multi_projective(encoding->bn.BaseCurve.generator, exp2, encoding->bn.pbits, encoding->bn.BaseCurve.lnggorder, encodedEltAff2, &(encoding->bn.BaseCurve), &encoding->BignumCtx);
	assert(okay);
	
	// Convert to projective
	digit_t temps[5000];
	okay = ecaffine_projectivize(encodedEltAff1, encodedEltProj1, &encoding->bn.BaseCurve, temps, &encoding->BignumCtx);
	assert(okay);
	okay = ecaffine_projectivize(encodedEltAff2, encodedEltProj2, &encoding->bn.BaseCurve, temps, &encoding->BignumCtx);
	assert(okay);

	encoding->add(encodedEltProj1, encodedEltProj2, result);

	delete encoding;
}

// This passes too!
// Same as above, but using projectivized generator for one of the points
void runEncodingTest2() {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	digit_t temps[5000];
	LEncodedElt lGen;		// BaseCurve generator in projective coordinates

	FieldElt zero, one;
	field->zero(zero);
	field->one(one);

	BOOL OK = ecaffine_projectivize(encoding->bn.BaseCurve.generator, lGen, &encoding->bn.BaseCurve, temps, &encoding->BignumCtx);
	assert(OK);

	LEncodedElt lG0;	// lGen^0
	OK = ecproj_single_exponentiation(lGen, zero, encoding->bn.pbits, encoding->bn.BaseCurve.lnggorder, lG0, &encoding->bn.BaseCurve, &encoding->BignumCtx);
	assert(OK);
	
	// Pick exponents
	FieldElt exp2;
	field->assignRandomElt(&exp2);

	// Apply it to the generator.  Result should be in affine coordinates
	LEncodedElt encodedEltAff2, encodedEltProj2, result;
	BOOL okay = ecaffine_exponentiation_via_multi_projective(encoding->bn.BaseCurve.generator, exp2, encoding->bn.pbits, encoding->bn.BaseCurve.lnggorder, encodedEltAff2, &(encoding->bn.BaseCurve), &encoding->BignumCtx);
	assert(okay);
	
	// Convert it to projective
	okay = ecaffine_projectivize(encodedEltAff2, encodedEltProj2, &encoding->bn.BaseCurve, temps, &encoding->BignumCtx);
	assert(okay);

	encoding->add(lG0, encodedEltProj2, result);

	delete encoding;
}

// Same as above, but using projectivized generator for both of the points
void runEncodingTest3() {
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	digit_t temps[5000];
	LEncodedElt lGen;		// BaseCurve generator in projective coordinates

	FieldElt zero, one;
	field->zero(zero);
	field->one(one);

	BOOL OK = ecaffine_projectivize(encoding->bn.BaseCurve.generator, lGen, &encoding->bn.BaseCurve, temps, &encoding->BignumCtx);
	assert(OK);

	LEncodedElt lG0;	// lGen^0
	OK = ecproj_single_exponentiation(lGen, zero, encoding->bn.pbits, encoding->bn.BaseCurve.lnggorder, lG0, &encoding->bn.BaseCurve, &encoding->BignumCtx);
	assert(OK);
	
	LEncodedElt lG1;	// lGen^1
	OK = ecproj_single_exponentiation(lGen, one, encoding->bn.pbits, encoding->bn.BaseCurve.lnggorder, lG1, &encoding->bn.BaseCurve, &encoding->BignumCtx);
	assert(OK);

	LEncodedElt result;
	encoding->add(lG0, lG1, result);

	delete encoding;
}

void runEncodingTest4() {
	const int MAX_MEM = 5;
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	
	// Pick exponent
	FieldElt exp1;
	field->assignRandomElt(&exp1);
	//field->set(exp1, 123);
	/*
	memset(exp1, 0, sizeof(FieldElt));
	exp1[0] = 123;
	exp1[1] = 1234;
	exp1[2] = 698;
	exp1[3] = 1;
	*/

	printf("NUM_TESTS = %d\n", NUM_TESTS);

	LEncodedElt encodedElt1;

	Timer* total = timers.newTimer("ExpTest", NULL);
	total->start();

	Timer* oldT = timers.newTimer("Old", "ExpTest");
	Timer* newT = timers.newTimer("New", "ExpTest");

	oldT->start();
	TIME(encoding->precomputePowersSlow(), "PreComp", "Old");
	oldT->stop();
	newT->start();
	TIME(encoding->precomputePowersFast(NUM_TESTS, MAX_MEM, false), "PreComp3", "New");
	newT->stop();

	Timer* t = timers.newTimer("EncodeOld", "Old");
	Timer* t3 = timers.newTimer("EncodeNew", "New");
	
	for (int i = 0; i < NUM_TESTS; i++) {
		field->assignRandomElt(&exp1);
		oldT->start();
		t->start();
		encoding->encodeSlow(exp1, encodedElt1);
		t->stop();
		oldT->stop();

		newT->start();
		t3->start();
		encoding->encode(exp1, encodedElt1);
		t3->stop();
		newT->stop();
	}
	
	total->stop();
	timers.printStats();	
	delete encoding;
}


#endif // DEBUG_ENCODING


void IOtest(variables_map& config) {
	bigctx_t BignumCtx;
	bigctx_t* pbigctx;

	pbigctx = &BignumCtx;
	memset(pbigctx, 0, sizeof(bigctx_t));

	int numTrials = config["trials"].as<int>();
	printf("Using %d trials\n", numTrials);

	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();

	FieldElt* field1 = new FieldElt[numTrials];
	FieldElt* smallExp = new FieldElt[numTrials];

	for (int i = 0; i < numTrials; i++) {
		field->assignRandomElt(&field1[i]);
	}

	//// Create some smaller exponents
	for (int i = 0; i < numTrials; i++) {
		field->zero(smallExp[i]);
		smallExp[i][0] = rand();
	}

	Timer* total = timers.newTimer("Total", NULL);
	total->start();

	FieldElt tmp;
	FieldElt sum;
	LEncodedElt elt1, elt2;
	field->zero(sum);
	Timer* fieldT = timers.newTimer("Field", "Total");
	fieldT->start();
	for (int trial = 0; trial < numTrials; trial++) {
		field->mul(field1[trial], smallExp[trial], tmp);
		field->add(sum, tmp, sum);
	}
	encoding->encodeSlowest(sum, elt1);
	fieldT->stop();

	digit_t dtmp[5];
	int sumLen = 5 + (numTrials - 1)/64 + 1;
	digit_t* dsum = new digit_t[sumLen];
	memset(dsum, 0, sizeof(digit_t) * sumLen);
	DWORDREG sumDigitLen = 1;

	Timer* bigT = timers.newTimer("BigInt", "Total");
	bigT->start();
	for (int trial = 0; trial < numTrials; trial++) {
		BOOL OK = multiply(field1[trial], 4, smallExp[trial], 1, dtmp, PBIGCTX_PASS);
		assert(OK);
		OK = add_full(dtmp, 5, dsum, sumDigitLen, dsum, sumLen, &sumDigitLen, PBIGCTX_PASS);		
		assert(OK);
	}

#ifndef DEBUG_ENCODING
	FieldElt fsum;
	to_modular(dsum, sumDigitLen, fsum, encoding->p, PBIGCTX_PASS);		
	BOOL OK = ecproj_single_exponentiation(encoding->lGen, fsum, encoding->bn.pbits, encoding->bn.BaseCurve.lnggorder, elt2, &encoding->bn.BaseCurve, PBIGCTX_PASS);
	assert(OK);
	bigT->stop();
#endif

	assert(encoding->equals(elt1, elt2));

	total->stop();

	if (config.count("raw")) {
		timers.printRaw();
	} else {
		timers.printStats();
	}
	printf("\n");

	delete encoding;
	delete [] field1;
	delete [] smallExp;
	delete dsum;
}
	




// Heat up the cache with our proof and IO, on the assumption that we just read it from a file or the network
// Compensates for running the worker on the same machine as the client
void warmCache(variables_map& config, Circuit* circuit, Proof* p, QAP* qap, Keys* keys) {		
	Timer* cacheWarming = timers.newTimer("CacheWarming", "Total");
	cacheWarming->start();
	volatile digit_t sum = 0;
	int fieldEltLen = sizeof(FieldElt)/sizeof(digit_t);
	int lEncodedEltLen = sizeof(LEncodedElt)/sizeof(digit_t);
	int rEncodedEltLen = sizeof(REncodedElt)/sizeof(digit_t);

	for (int i = 0; i < (int) circuit->inputs.size()+1; i++) {
		if (config.count("dv")) {
			for (int j = 0; j < fieldEltLen; j++) {
				sum += p->inputs[i][j];
				sum += keys->sk->v[i+1][j];			
			}
		}

		if (config.count("pv")) {
			for (int j = 0; j < fieldEltLen; j++) {
				sum += p->inputs[i][j];
			}
			for (int j = 0; j < lEncodedEltLen; j++) {
#ifdef USE_OLD_PROTO
				sum += keys->pk->V[i+1][j];
#else // !USE_OLD_PROTO
				sum += keys->pk->Vpolys[i+1][j];
#endif // USE_OLD_PROTO
			}
		}
	}

	for (int i = 0; i < circuit->outputs.size(); i++) {
		if (config.count("dv")) {
			for (int j = 0; j < fieldEltLen; j++) {
				sum += p->outputs[i][j];
				sum += keys->sk->v[i + circuit->inputs.size() + 1][j];			
			}
		}

		if (config.count("pv")) {		
			for (int j = 0; j < fieldEltLen; j++) {
				sum += p->outputs[i][j];
			}
			for (int j = 0; j < lEncodedEltLen; j++) {
#ifdef USE_OLD_PROTO
				sum += keys->pk->V[qap->polyIndex((GateMul2*)circuit->outputs[i]->input)][j];
#else  // !USE_OLD_PROTO
				sum += keys->pk->Vpolys[qap->polyIndex((GateMul2*)circuit->outputs[i]->input)][j];
#endif // USE_OLD_PROTO
			}
		}
	}

	for (int i = 0; i < sizeof(Proof) / sizeof(digit_t); i++) {
		sum += ((digit_t*)p)[i];
	}

#ifndef USE_OLD_PROTO
	if (config.count("dv")) {
		for (int i = 0; i < fieldEltLen; i++) {
			sum += ((QapSecretKey*)(keys->sk))->rY[i];
			sum += keys->sk->target[i];
			sum += keys->sk->alphaV[i];
			sum += ((QapSecretKey*)(keys->sk))->alphaY[i]; 
			sum += keys->sk->gamma[i];
			sum += keys->sk->beta[i];
		}
	}

	if (config.count("pv")) {
		for (int i = 0; i < lEncodedEltLen; i++) {
			sum += (*keys->pk->alphaW)[i];
			sum += (*keys->pk->betaGammaL)[i];
			sum += ((QapPublicKey*)keys->pk)->Ypolys[0][i];
		}

		for (int i = 0; i < rEncodedEltLen; i++) {
			sum += (*keys->pk->RtAtS)[i];
			sum += (*keys->pk->alphaV)[i];
			sum += (*((QapPublicKey*)keys->pk)->alphaY)[i];
			sum += (*keys->pk->gammaR)[i];
			sum += (*keys->pk->betaGammaR)[i];
			sum += keys->pk->Wpolys[0][i];
		}
	}
#endif 

	printf("Cache sum is %ld\n", sum);
	cacheWarming->stop();
}

/****************************************************************************
											Actual Performance Tests
 ****************************************************************************/

/****** Matrix Perf Test.  Generates a circuit for matrix multiplication and executes it ******/
void runMatrixPerfTest(variables_map& config) {
	int perfN = config["matrix"].as<int>();
	printf("Using N = %d\n", perfN);
	Timer* totalTimer = timers.newTimer("Total", NULL);
	Timer* oneTimeWorkTimer = timers.newTimer("OneTimeWork", NULL);
	totalTimer->start();

	// Create some circuits to test
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	CircuitMatrixMultiply stdMatMulCircuit(field, perfN);
	CircuitMatrixMultiply qapMatMulCircuitNadder(field, perfN, false);

	
	// Assign random inputs
	if (config.count("bits")) {
		printf("Assigning random %d-bit input values\n", config["bits"].as<int>());
		assignRandom(field, qapMatMulCircuitNadder.inputs, config["bits"].as<int>());
	} else {
		printf("Assigning full %d-bit input values\n", sizeof(FieldElt)*8);
		assignRandom(field, qapMatMulCircuitNadder.inputs); 
	}
	assignConstOne(field, qapMatMulCircuitNadder.inputs, qapMatMulCircuitNadder.numTrueInputs()); // Make sure the QAP's final input is a 1

	// Assign both circuits the same random inputs
	for (int i = 0; i < stdMatMulCircuit.inputs.size(); i++) {
		field->copy(qapMatMulCircuitNadder.inputs[i]->value, stdMatMulCircuit.inputs[i]->value);
	}

	// Time the basic circuit evaluation
	TIME(stdMatMulCircuit.eval(false), "StdEval", "Total");
	TIME(qapMatMulCircuitNadder.eval(false), "QAPEval", "Total");

	// Construct the QAP
	Timer* constructionTime = timers.newTimer("Construct", "Total");
	constructionTime->start();
	QAP qap(&qapMatMulCircuitNadder, encoding, config.count("pcache") > 0);	
	constructionTime->stop();

	if (config.count("raw")) {
		printf("\nCircInputs: %d\n", qap.circuit->inputs.size());
		printf("CircOutputs: %d\n", qap.circuit->outputs.size());
		printf("CircGates: %d\n", qap.circuit->gates.size());
		printf("CircMulGates: %d\n", qap.circuit->mulGates.size());
		printf("QAPsize: %d\n", qap.getSize());
		printf("QAPdegree: %d\n", qap.getDegree());
	} else {
		printf("Number of QAP inputs: %d, mult gates: %d\nQAP size: %d, degree: %d\n", qap.circuit->inputs.size(), qap.circuit->mulGates.size(), qap.getSize(), qap.getDegree());
	}

	fflush(stdout);

	Keys* keys = NULL;
	TIME(keys = qap.genKeys(config), "KeyGen", "Total");
	printf("KeyStats:\n");
	encoding->print_stats(); encoding->reset_stats();
	Proof* p = NULL;
	TIME(p = qap.doWork(keys->pk, config), "DoWork", "Total");
	printf("Work:\n");
	encoding->print_stats(); encoding->reset_stats();
	printf("Work complete\n");  fflush(stdout);
	// Warm Cache
	qap.verify(keys, p, config, false);

	bool success;
	TIME(success = qap.verify(keys, p, config), "Verify", "Total");
	printf("Verification complete\n");  fflush(stdout);
	printf("Verify:\n");
	encoding->print_stats(); encoding->reset_stats();

	assert(success);
	totalTimer->stop();

	printf("\n");
	keys->printSizes(config.count("raw"));
	if (config.count("raw")) {
		timers.printRaw();
	} else {
		timers.printStats();
	}

	delete encoding;
	delete keys;
	delete p;
	
}


typedef std::map<long long, WrappedElt> WireValues;
typedef std::pair<long long, WrappedElt> WireValue;

/***********  Helpers for interacting with wire values in files ****************/
void parseWireFile(Field* field, string file, WireValues& wireValues) {
	ifstream ifs ( file , ifstream::in );
	if(!ifs.good()) {
		printf("Unable to open wire file: %s\n", file.c_str());
		exit(6);
	} else {
		printf("Reading wire values from file: %s\n", file.c_str());
	}

	string line;
	long long wireId = 0;
	char buffer[100];
	while (getline(ifs, line)) {
		WrappedElt val;
		
		memset(buffer, 0, sizeof(buffer));
		if (2 == sscanf_s(line.c_str(), "%lld %s", &wireId, buffer, sizeof(buffer))) {
			scan_io_value(buffer, (unsigned char*)val.elt, sizeof(val.elt));
			wireValues.insert(WireValue(wireId, val));
		}
	}

	ifs.close();
}

void writeWires(WireVector& wires, string filename) {
	char valBuff[1000];
	ofstream ofs(filename, ios::out);
	for (WireVector::iterator iter = wires.begin();
	    iter != wires.end();
			iter++) {
		ofs << (*iter)->id << " ";		
		emit_io_value(valBuff, sizeof(valBuff), (unsigned char*)(*iter)->value, sizeof(FieldElt));
		ofs.write(valBuff, sizeof(FieldElt)*2);
		ofs << "\n";
		//ofs << valBuff << "\n";
	}
}

void writeInput(variables_map& config, Circuit& circuit) {
	if (config.count("input") > 0) {
		writeWires(circuit.inputs, config["input"].as<string>());
	} else {
		printf("No input file name specified, so not writing out the input values\n");
	}
}

void writeOutput(variables_map& config, Circuit& circuit) {
	if (config.count("output") > 0) {
		writeWires(circuit.outputs, config["output"].as<string>());
	} else {
		printf("No output file name specified, so not writing out the input values\n");
	}
}


bool parseAndAssignInput(variables_map& config, Circuit& circuit, WireValues& wireValues) {
	if (config.count("input")) { // && config.count("output")) {
		parseWireFile(circuit.field, config["input"].as<string>(), wireValues);		
	} else {
		return false;
	}
	
	// Apply the inputs
	for (WireVector::iterator iter = circuit.inputs.begin();
	     iter != circuit.inputs.end();
			 iter++) {
		WireValues::iterator lookup = wireValues.find((*iter)->id);
		assert(lookup != wireValues.end());	// If this triggers, we failed to find a value for an input wire!
		circuit.field->copy(lookup->second.elt, (*iter)->value);
	}

	return true;
}

/****** Perf test based on circuit description from a file ******/
void runPerfTest(variables_map& config) {
	printf("Using circuit from file %s\n", config["file"].as<string>().c_str());
	Timer* totalTimer = timers.newTimer("Total", NULL);
	totalTimer->start();

	// Create the circuit
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	Circuit circuit(field, config["file"].as<string>());

	WireValues wireValues;

	if (config.count("bits")) {  
		printf("Assigning random %d-bit input values\n", config["bits"].as<int>());
		assignRandom(field, circuit.inputs, config["bits"].as<int>());
		assignConstOne(field, circuit.inputs, circuit.numTrueInputs()); // Make sure the QAP's final input is a 1
		writeInput(config, circuit);	// Save the random input, in case we need it
	} else if (!parseAndAssignInput(config, circuit, wireValues)) { // Parsing the input file failed
		printf("Assigning full %d-bit input values\n", sizeof(FieldElt)*8);
		assignRandom(field, circuit.inputs); 
		assignConstOne(field, circuit.inputs, circuit.numTrueInputs()); // Make sure the QAP's final input is a 1
		writeInput(config, circuit);  // Save the random input, in case we need it
	}

	fflush(stdout);

	// Time the basic circuit evaluation
	TIME(circuit.eval(false), "StdEval", "Total");

	// Construct the QAP
	Timer* constructionTime = timers.newTimer("Construct", "Total");
	constructionTime->start();
	QAP qap(&circuit, encoding, config.count("pcache") > 0);	
	constructionTime->stop();

//#define DEBUG_PCACHE 1

#ifdef DEBUG_PCACHE
	Circuit circuit2(field, config["file"].as<string>());
	QAP qap2(&circuit2, encoding, !(config.count("pcache") > 0));	
	// Compare the polynomials in each QAP
	for (int i = 0; i < qap2.getSize() + 1; i++) {
		if (!qap.V[i].equal(field, qap2.V[i])) {
			printf("Poly V[%d] differs!\n", i);
		}
		if (!qap.W[i].equal(field, qap2.W[i])) {
			printf("\nPoly W[%d] differs!\n", i);
			printf("circuit's version: \n ");
			qap.W[i].print(field);
			printf("\ncircuit2's version: \n ");
			qap2.W[i].print(field);
		}
		if (!qap.Y[i].equal(field, qap2.Y[i])) {
			printf("Poly Y[%d] differs!\n", i);
		}
	}
	printf("Done comparing polynomials\n");
#endif

	if (config.count("raw")) {
		printf("\nCircInputs: %d\n", qap.circuit->inputs.size());
		printf("CircOutputs: %d\n", qap.circuit->outputs.size());
		printf("CircGates: %d\n", qap.circuit->gates.size());
		printf("CircMulGates: %d\n", qap.circuit->mulGates.size());
		printf("QAPsize: %d\n", qap.getSize());
		printf("QAPdegree: %d\n", qap.getDegree());
	} else {
		printf("Number of QAP inputs: %d, mult gates: %d\nQAP size: %d, degree: %d\n", qap.circuit->inputs.size(), qap.circuit->mulGates.size(), qap.getSize(), qap.getDegree());
	}
	fflush(stdout);

	Keys* keys = NULL;
	TIME(keys = qap.genKeys(config), "KeyGen", "Total");
	//((QapPublicKey*)keys->pk)->print(encoding);

	Timer* fileTimer = NULL;
	if (config.count("keys")) {
		fileTimer = timers.newTimer("FileIO", "Total");
		fileTimer->start();
		FileArchiver* file = new FileArchiver(config["keys"].as<string>() + ".pub", FileArchiver::Write);
		Archive* arc = new Archive(file, encoding);
		keys->pk->serialize(arc);
		delete arc;
		delete file;

#ifdef _DEBUG
		// Debugging: Try reading it back in
		file = new FileArchiver(config["keys"].as<string>() + ".pub", FileArchiver::Read);
		arc = new Archive(file, encoding);
		PublicKey* pk_orig = keys->pk;		
		keys->pk = new QapPublicKey();
		keys->pk->deserialize(arc);		
		// Check for equality
		if (!((QapPublicKey*)keys->pk)->equals(encoding, (QapPublicKey*)pk_orig)) {
			printf("Deserialized key differs from the original!\n");
		}

		delete pk_orig;
		delete arc;
		delete file;
#endif

		file = new FileArchiver(config["keys"].as<string>() + ".priv", FileArchiver::Write);
		arc = new Archive(file, encoding);
		keys->sk->serialize(arc);		
		delete arc;
		delete file;
		fileTimer->stop();

#ifdef _DEBUG
		// Debugging: Try reading it back in
		file = new FileArchiver(config["keys"].as<string>() + ".priv", FileArchiver::Read);
		arc = new Archive(file, encoding);
		delete keys->sk;
		keys->sk = new QapSecretKey();
		keys->sk->deserialize(arc);				
		delete arc;
		delete file;
#endif
	}

	Proof* p = NULL;
	TIME(p = qap.doWork(keys->pk, config), "DoWork", "Total");
	//encoding->print(p->V);  printf("\n");
	//encoding->print(p->W);  printf("\n");
	//encoding->print(p->Y);  printf("\n");
	//encoding->print(p->alphaV);  printf("\n");
	//encoding->print(p->alphaW);  printf("\n");
	//encoding->print(p->alphaY);  printf("\n");
	//encoding->print(p->H);	  printf("\n");
	//encoding->print(p->beta);	printf("\n");

	if (config.count("keys")) {
		fileTimer->start();
		FileArchiver* file = new FileArchiver(config["keys"].as<string>() + ".proof", FileArchiver::Write);
		Archive* arc = new Archive(file, encoding);
		p->serialize(arc);

		delete arc;
		delete file;
		fileTimer->stop();

#ifdef _DEBUG
		// Debugging: Try reading it back in
		file = new FileArchiver(config["keys"].as<string>() + ".proof", FileArchiver::Read);
		arc = new Archive(file, encoding);
		Proof* p_orig = p;
		//delete p;
		p = new Proof();
		p->deserialize(arc);		

		// Check for equality
		if (!p->equal(encoding, p_orig)) {
			printf("Deserialized proof differs from the original!\n");
		}

		delete arc;
		delete file;
		delete p_orig;
#endif
	}

	// Warm Cache
	qap.verify(keys, p, config, false);
	
	bool success;
	TIME(success = qap.verify(keys, p, config), "Verify", "Total");
	assert(success);
	totalTimer->stop();
	
	if (success) {
		printf("Proof verifies!\n");
	} else {
		printf("Proof failed!\n");
	}

#ifdef _DEBUG
	// Make sure a bad proof doesn't verify. TODO: Make this a more comprehensive test
	p->outputs[0][0] = p->outputs[0][0] + 5;
	success = qap.verify(keys, p, config);
	if (success) {
		printf("Warning - Bad proof passed!\n");
		assert(false);
	} else {
		printf("Successfully caught a bad proof\n");
	}
#endif


	if (config.count("output")) {
		writeOutput(config, circuit);
	}

	printf("\n");
	keys->printSizes(config.count("raw"));

	if (config.count("raw")) {
		timers.printRaw();
	} else {
		timers.printStats();
	}

	delete encoding;
	delete keys;
	delete p;
	
}

// Demonstrate that the networking code is functional
void runNetworkTest(variables_map& config) {
	printf("Using circuit from file %s\n", config["file"].as<string>().c_str());

	// Create the circuit
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	Circuit circuit(field, config["file"].as<string>());

	WireValues wireValues;

	if (!parseAndAssignInput(config, circuit, wireValues)) {
		// Assign random inputs
		assignRandom(field, circuit.inputs);	
		assignConstOne(field, circuit.inputs, circuit.numTrueInputs()); // Make sure the QAP's final input is a 1
	}

	circuit.eval(false);
	
	// Construct the QAP
	QAP qap(&circuit, encoding, config.count("pcache") > 0);	

	printf("Number of QAP inputs: %d, mult gates: %d\nQAP size: %d, degree: %d\n", qap.circuit->inputs.size(), qap.circuit->mulGates.size(), qap.getSize(), qap.getDegree());

	Keys* keys = NULL;

	if (config.count("client")) {
		Network net;
    Proof p((int)circuit.inputs.size(), (int)circuit.outputs.size());

		net.startClient(config["client"].as<string>());

		keys = qap.genKeys(config);

		Archive arc(&net, encoding);
		keys->pk->serialize(&arc);


		// Send inputs    
		for (int i = 0; i < (int)circuit.inputs.size(); i++) {
			arc.write(circuit.inputs[i]->value);
      field->copy(circuit.inputs[i]->value, p.inputs[i]);
		}

		p.deserialize(&arc);

		// Get the outputs		
		arc.read(p.outputs, (int) circuit.outputs.size());

		bool success = qap.verify(keys, &p, config);

#ifdef _DEBUG
		// Do the work locally too and compare
		Proof* p2 = qap.doWork(keys->pk, config);
		delete p2;
#endif

		if (success) {
			printf("Proof from the server checks out!\n");
		} else {
			printf("Server tried to cheat us!\n");
		}

		assert(success);
		delete keys;
	} else {
		assert(config.count("server"));
		Network net;
		net.startServer();
		Archive arc(&net, encoding);

		QapPublicKey pk;
		pk.deserialize(&arc);

		pk.qap = &qap;	// We should have built the same QAP the client did

		// Receive inputs
		for (int i = 0; i < (int)circuit.inputs.size(); i++) {
			arc.read(circuit.inputs[i]->value);
		}


		Proof* p = qap.doWork(&pk, config);
		p->serialize(&arc);

		// Send the outputs
		arc.write(p->outputs, p->numOutputs);		

		delete p;
	}

	delete encoding;	
}



/****** Verify Perf Test.  Only build enough to test verification ******/
void runVerifyTest(variables_map& config) {
	int perfN = config["matrix"].as<int>();
	printf("Using N = %d\n", perfN);
	Timer* totalTimer = timers.newTimer("Total", NULL);
	Timer* oneTimeWorkTimer = timers.newTimer("OneTimeWork", NULL);
	totalTimer->start();
			
	printf("\nTESTING VERIFICATION.  TIMES ARE NOT TO BE TRUSTED FOR OTHER PURPOSES\n\n");

	// Create a circuit to test
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	CircuitMatrixMultiply qapMatMulCircuitNadder(field, perfN, false);
	
	// Assign random inputs
	if (config.count("bits")) {
		printf("Assigning random %d-bit input values\n", config["bits"].as<int>());
		assignRandom(field, qapMatMulCircuitNadder.inputs, config["bits"].as<int>());
	} else {
		printf("Assigning full %d-bit input values\n", sizeof(FieldElt)*8);
		assignRandom(field, qapMatMulCircuitNadder.inputs); 
	}
	assignConstOne(field, qapMatMulCircuitNadder.inputs, qapMatMulCircuitNadder.numTrueInputs()); // Make sure the QAP's final input is a 1

	// Time the basic circuit evaluation
	TIME(qapMatMulCircuitNadder.eval(false), "QAPEval", "Total");

	// Construct the QAP
	Timer* constructionTime = timers.newTimer("Construct", "Total");
	constructionTime->start();
	QAP qap(&qapMatMulCircuitNadder, encoding, config.count("pcache") > 0);	
	constructionTime->stop();

	if (config.count("raw")) {
		printf("\nCircInputs: %d\n", qap.circuit->inputs.size());
		printf("CircOutputs: %d\n", qap.circuit->outputs.size());
		printf("CircGates: %d\n", qap.circuit->gates.size());
		printf("CircMulGates: %d\n", qap.circuit->mulGates.size());
		printf("QAPsize: %d\n", qap.getSize());
		printf("QAPdegree: %d\n", qap.getDegree());
	} else {
		printf("Number of QAP inputs: %d, mult gates: %d\nQAP size: %d, degree: %d\n", qap.circuit->inputs.size(), qap.circuit->mulGates.size(), qap.getSize(), qap.getDegree());
	}

	fflush(stdout);

	Keys* keys = NULL;
	TIME(keys = qap.genKeys(config), "KeyGen", "Total");

	Proof* p = new Proof((int)qapMatMulCircuitNadder.inputs.size(), (int)qapMatMulCircuitNadder.outputs.size());

	// Copy over the inputs and outputs
	for (int i = 0; i < (int) qapMatMulCircuitNadder.inputs.size(); i++) {
		field->copy(qapMatMulCircuitNadder.inputs[i]->value, p->inputs[i]);
	}

	for (int i = 0; i < qapMatMulCircuitNadder.outputs.size(); i++) {
		field->copy(qapMatMulCircuitNadder.outputs[i]->value, p->outputs[i]);
	}


	bool success;
	TIME(success = qap.verify(keys, p, config), "Verify", "Total");

	totalTimer->stop();

	printf("\n");
	keys->printSizes(config.count("raw"));
	if (config.count("raw")) {
		timers.printRaw();
	} else {
		timers.printStats();
	}

	delete encoding;
	delete keys;
	delete p;
	
}

/****** Microbenchmarks ******/
// Includes: 
//	Field add/sub/mul/div
//  Encoding (Base/Twist) mult, multiMul, single-base exp
//  Poly interp/mul
void runMicroTests(variables_map& config) {
	int numTrials = config["trials"].as<int>();
	string option = config["micro"].as<string>();
	printf("Running numTrials = %d\n", numTrials);

	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();

	// Preallocate all of the memory we'll need
	FieldElt* field1 = new FieldElt[numTrials];
	FieldElt* field2 = new FieldElt[numTrials];
	FieldElt* smallExp = new FieldElt[numTrials];
	FieldElt* binaryExp = new FieldElt[numTrials];
	
	LEncodedElt* lencodings = new LEncodedElt[numTrials];
	LEncodedElt* lencodings2 = new LEncodedElt[numTrials];
	REncodedElt* rencodings = new REncodedElt[numTrials];
	REncodedElt* rencodings2 = new REncodedElt[numTrials];
	LCondensedEncElt* lcondensed = new LCondensedEncElt[numTrials];
	RCondensedEncElt* rcondensed = new RCondensedEncElt[numTrials];
	multi_exponent_t* multiExpL = new multi_exponent_t[numTrials];
	multi_exponent_t* multiExpR = new multi_exponent_t[numTrials];

	EncodedProduct* prod = new EncodedProduct[numTrials];

	for (int i = 0; i < numTrials; i++) {
		field->assignRandomElt(&field1[i]);
		field->assignRandomElt(&field2[i]);
	}

	Timer* total = timers.newTimer("Total", NULL);
	total->start();

	/////////////// Field ops ///////////////////////////
	if (option.compare("all") == 0 || option.compare("field") == 0 || option.compare("encoding") == 0) {
		TIMEREP(field->add(field1[i], field2[i], field1[i]), numTrials, "FieldAdd", "Total");
		TIMEREP(field->sub(field1[i], field2[i], field1[i]), numTrials, "FieldSub", "Total");
		TIMEREP(field->mul(field1[i], field2[i], field2[i]), numTrials, "FieldMul", "Total");
		TIMEREP(field->mul(field2[i], field1[i], field2[i]), numTrials, "FieldDiv", "Total");
	}

	/////////////// Encoding ops ///////////////////////////
	if (option.compare("all") == 0 || option.compare("encoding") == 0) {
		TIME(encoding->precomputePowersSlow(), "PrecomputePwrSlow", "Total");
		TIME(encoding->precomputePowersFast(numTrials*2, config["mem"].as<int>(), false), "PrecomputePwrFast", "Total");	// x2 for L and R

		TIMEREP(encoding->encodeSlowest(field1[i], lencodings[i]), numTrials, "EncodeSlowestL", "Total");
		TIMEREP(encoding->encodeSlow(field1[i], lencodings2[i]), numTrials, "EncodeSlowL", "Total");
		TIMEREP(encoding->encode(field1[i], lencodings[i]), numTrials, "EncodeFastL", "Total");

		TIMEREP(encoding->encodeSlowest(field1[i], rencodings[i]), numTrials, "EncodeSlowestR", "Total");
		TIMEREP(encoding->encodeSlow(field1[i], rencodings2[i]), numTrials, "EncodeSlowR", "Total");
		TIMEREP(encoding->encode(field1[i], rencodings[i]), numTrials, "EncodeFastR", "Total");

		printf("Finished encoding\n");  fflush(stdout);

		// Same as EncodeSlowest{L,R}
		//TIMEREP(encoding->mul(lencodings[i], field1[i], lencodings[i]), numTrials, "ConstMulL", "Total");
		//TIMEREP(encoding->mul(rencodings[i], field1[i], rencodings[i]), numTrials, "ConstMulR", "Total");

		TIMEREP(encoding->mul(lencodings[i], rencodings[i], prod[i]), numTrials, "Pairing", "Total");

		TIMEREP(encoding->add(lencodings[i], lencodings2[i], lencodings2[i]), numTrials, "EncAddL", "Total");
		TIMEREP(encoding->add(rencodings[i], rencodings2[i], rencodings2[i]), numTrials, "EncAddR", "Total");

		printf("Finished EncAdd\n");  fflush(stdout);
		//TIMEREP(encoding->doubleIt(lencodings[i], lencodings2[i]), numTrials, "EncDblL", "Total");
		//TIMEREP(encoding->doubleIt(rencodings[i], rencodings2[i]), numTrials, "EncDblR", "Total");

		// Setup an old multi op
		/*
		for (int i = 0; i < numTrials; i++) {
			multiExpL[i].pbase = lencodings[i];
			multiExpL[i].pexponent = field1[i];
						
			multiExpL[i].lng_bits = sizeof(FieldElt) * 8 -2;
			multiExpL[i].lng_pexp_alloc =	(int) ceil((double)multiExpL[i].lng_bits / (sizeof(digit_t) * 8));		
			multiExpL[i].offset_bits = 0;

			multiExpR[i].pbase = rencodings[i];
			multiExpR[i].pexponent = field1[i];
						
			multiExpR[i].lng_bits = sizeof(FieldElt) * 8 -2;
			multiExpR[i].lng_pexp_alloc =	(int) ceil((double)multiExpL[i].lng_bits / (sizeof(digit_t) * 8));		
			multiExpR[i].offset_bits = 0;
		}

		TIME(encoding->multiMulOld(multiExpL, numTrials, lencodings[0]), "MultiMulOldL", "Total");
		TIME(encoding->multiMulOld(multiExpR, numTrials, rencodings[0]), "MultiMulOldR", "Total");
		*/

		// Setup a new multi op
		MultiExpPrecompTable<LEncodedElt> lTable;
		MultiExpPrecompTable<REncodedElt> rTable;
		TIME(encoding->precomputeMultiPowers(lTable, lencodings, numTrials, 1, config["mem"].as<int>(), config.count("fill") != 0), "MultiMulNewPreL", "Total");
		TIME(encoding->precomputeMultiPowers(rTable, rencodings, numTrials, 1, config["mem"].as<int>(), config.count("fill") != 0), "MultiMulNewPreR", "Total");

		printf("Finished precomp\n");  fflush(stdout);

		TIME(encoding->multiMul(lTable, field1, numTrials, lencodings[0]), "MultiMulNewL", "Total");
		TIME(encoding->multiMul(rTable, field1, numTrials, rencodings[0]), "MultiMulNewR", "Total");

		printf("Finished MultiMulNew\n");  fflush(stdout);

		//// Try with smaller exponents
		for (int i = 0; i < numTrials; i++) {
			field->zero(smallExp[i]);
			smallExp[i][0] = rand();

			if (rand() % 2 == 0) {
				field->zero(binaryExp[i]);
			} else {
				field->one(binaryExp[i]);
			}
		}

		printf("Finished creating smaller exponents\n");  fflush(stdout);

		TIME(encoding->multiMul(lTable, smallExp, numTrials, lencodings[0]), "MultiMulNewSmallL", "Total");
		TIME(encoding->multiMul(rTable, smallExp, numTrials, rencodings[0]), "MultiMulNewSmallR", "Total");

		printf("Finished exp with small exp\n");  fflush(stdout);

		TIME(encoding->multiMul(lTable, binaryExp, numTrials, lencodings[0]), "MultiMulNewBinaryL", "Total");
		TIME(encoding->multiMul(rTable, binaryExp, numTrials, rencodings[0]), "MultiMulNewBinaryR", "Total");

		printf("Finished exp with binary exp\n");  fflush(stdout);

		// Clean up tables
		encoding->deleteMultiTables(lTable);
		encoding->deleteMultiTables(rTable);

		printf("Finished delete \n");  fflush(stdout);

		// Try compressing and decompressing the points
		TIME(encoding->compressMany(lencodings, lcondensed, numTrials, false), "CompressL", "Total");
		TIME(encoding->compressMany(rencodings, rcondensed, numTrials, false), "CompressR", "Total");
		TIME(encoding->decompressMany(lcondensed, lencodings, numTrials), "DecompressL", "Total");
		TIME(encoding->decompressMany(rcondensed, rencodings, numTrials), "DecompressR", "Total");

		printf("Finished compress/decompress\n");  fflush(stdout);
		
		// Test time to affinize our points (which are kept in projective)
		// Do this last, when we won't need these points any more!
		/*TIME(encoding->affinizeMany(lencodings, (LEncodedEltAff*)lencodings, numTrials, false), "AffinizeL", "Total");
		TIME(encoding->affinizeMany(rencodings, (REncodedEltAff*)rencodings, numTrials, false), "AffinizeR", "Total");

		TIME(encoding->projectivizeMany((LEncodedEltAff*)lencodings, lencodings, numTrials), "ProjectivizeL", "Total");
		TIME(encoding->projectivizeMany((REncodedEltAff*)lencodings, rencodings, numTrials), "ProjectivizeR", "Total");*/
	}

	/////////////// Poly Ops ///////////////////////////
	if (option.compare("all") == 0 || option.compare("poly-all") == 0 || option.compare("poly-fast") == 0 || option.compare("poly-slow") == 0) {
		Timer* polyPreComp = timers.newTimer("PolyPreComp", "Total");
		polyPreComp->start();
		PolyTree* tree = new PolyTree(field, field1, numTrials);
		FieldElt* denominators = Poly::genLagrangeDenominators(field, *tree->polys[tree->height-1][0], tree, field1, numTrials);
		polyPreComp->stop();

		if (option.compare("poly-all") == 0 || option.compare("poly-fast") == 0) {
			TIME(Poly::interpolate(field, field2, numTrials, tree, denominators), "PolyInterp", "Total");
		}
		if (option.compare("poly-all") == 0 || option.compare("poly-slow") == 0) {				 
			TIME(Poly::interpolateSlow(field, field1, field2, numTrials), "PolyInterSlow", "Total");
		}

		// Compare slow vs. optimized polynomial multiplication
		Poly p1(field), p2(field), r(field);
		polyRand(field, p1, numTrials);
		polyRand(field, p2, numTrials);
		if (option.compare("poly-all") == 0 || option.compare("poly-fast") == 0) {
			TIME(Poly::mul(p1, p2, p1), "PolyMul", "Total");
		}
		if (option.compare("poly-all") == 0 || option.compare("poly-slow") == 0) {				 
			TIME(Poly::mulSlow(p1, p2, p1), "PolyMulSlow", "Total");
		}

		//FieldElt tmp;
		//field->polyMakeMonic(p2, p2, tmp);	// polyMod expects a monic poly for a modulus
		//TIME(field->polyMod(p1, p2, p1, r), "PolyMod", "Total");
	}
	
	total->stop();

	if (config.count("raw")) {
		timers.printRaw();
	} else {
		timers.printStats();
	}
	printf("\n");

	delete encoding;
	delete [] field1;
	delete [] field2;
	delete [] smallExp;
	delete [] binaryExp;
	delete [] lencodings;
	delete [] lencodings2;
	delete [] rencodings;
	delete [] rencodings2;
	delete [] prod;
	delete [] multiExpL;
	delete [] multiExpR;
}

/****** Just execute the circuit with the inputs specified and write out the outputs ******/
void runUnitTest(variables_map& config) {
	printf("Unit testing circuit from file %s\n", config["file"].as<string>().c_str());

	// Create the circuit
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	Circuit circuit(field, config["file"].as<string>());

	// Parse the I/O
	WireValues wireValues;

	parseAndAssignInput(config, circuit, wireValues);

	circuit.eval(false);

	for (int i = 0; i < circuit.wires.size(); i++) {
		cout << i << " ";

		FieldElt tmp, zero;
		field->zero(zero);

		if (circuit.wires[i]->value[3] > 0) {	// Most likely a negative number
			cout << "-";
			field->sub(zero, circuit.wires[i]->value, tmp);
		} else {
			field->copy(circuit.wires[i]->value, tmp);
		}

		//char buffer[500];
		//emit_io_value(buffer, sizeof(buffer), (unsigned char*)circuit.wires[i]->value, sizeof(FieldElt));
		//cout << buffer << endl;
		cout << "0x" << hex << tmp[0] << dec << endl;
		
	}

	// Write out the output
	writeOutput(config, circuit);

	delete encoding;
}


void runGenKeys(variables_map& config) {
	printf("Using circuit from file %s\n", config["file"].as<string>().c_str());

	// Create the circuit
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	Circuit circuit(field, config["file"].as<string>());

	QAP qap(&circuit, encoding, config.count("pcache") > 0);	
	Keys* keys = qap.genKeys(config);
	//((QapPublicKey*)keys->pk)->print(encoding);

	FileArchiver* file = new FileArchiver(config["keys"].as<string>() + ".pub", FileArchiver::Write);
	Archive* arc = new Archive(file, encoding);
	keys->pk->serialize(arc);
	delete arc;
	delete file;

	file = new FileArchiver(config["keys"].as<string>() + ".priv", FileArchiver::Write);
	arc = new Archive(file, encoding);
	keys->sk->serialize(arc);
	delete arc;
	delete file;

	delete encoding;
	delete keys;	
}

void runDoWork(variables_map& config) {
	printf("Using circuit from file %s\n", config["file"].as<string>().c_str());
	
	// Create the circuit
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	Circuit circuit(field, config["file"].as<string>());

	WireValues wireValues;

	bool result = parseAndAssignInput(config, circuit, wireValues);
	assert(result);

	circuit.eval(false);

	// Construct the QAP
	QAP qap(&circuit, encoding, config.count("pcache") > 0);	

	//Keys* keys = qap.genKeys(config);	

	// Get the public key
	FileArchiver* file = new FileArchiver(config["keys"].as<string>() + ".pub", FileArchiver::Read);
	Archive* arc = new Archive(file, encoding);
	QapPublicKey pk;
	pk.deserialize(arc);		
	delete arc;
	delete file;
	//pk.print(encoding);

	// Do the work
	Proof* p = NULL;
	p = qap.doWork(&pk, config);
	//p = qap.doWork(keys->pk, config);

	// Save the outputs
	writeOutput(config, circuit);

	// Does it pass before we serialize?
	/*Keys* keys = new Keys(false, &qap, 1, 1, false);
	keys->pk = &pk;
	bool success = qap.verify(keys, p, config);*/

	// Save the proof
	file = new FileArchiver(config["keys"].as<string>() + ".proof", FileArchiver::Write);
	arc = new Archive(file, encoding);
	p->serialize(arc);

	// Does it pass after we serialize?
	//success = qap.verify(keys, p, config);

	//printf("Proof:\n");
	//encoding->print(p->V);  printf("\n");
	//encoding->print(p->W);  printf("\n");
	//encoding->print(p->Y);  printf("\n");
	//encoding->print(p->alphaV);  printf("\n");
	//encoding->print(p->alphaW);  printf("\n");
	//encoding->print(p->alphaY);  printf("\n");
	//encoding->print(p->H);	  printf("\n");
	//encoding->print(p->beta);	printf("\n");

	delete arc;
	delete file;

	delete encoding;
	delete p;	
}

void runVerify(variables_map& config) {
	printf("Using circuit from file %s\n", config["file"].as<string>().c_str());

	// Create the circuit
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	Circuit circuit(field, config["file"].as<string>());

	// Get the public key
	FileArchiver* file = new FileArchiver(config["keys"].as<string>() + ".pub", FileArchiver::Read);
	Archive* arc = new Archive(file, encoding);
	QapPublicKey pk;
	pk.deserialize(arc);		
	delete arc;
	delete file;
	//pk.print(encoding);
	
	// Get the proof
	file = new FileArchiver(config["keys"].as<string>() + ".proof", FileArchiver::Read);
	arc = new Archive(file, encoding);
	Proof proof;
	proof.deserialize(arc);		
	delete arc;
	delete file;

	//printf("Proof:\n");
	//encoding->print(proof.V);  printf("\n");
	//encoding->print(proof.W);  printf("\n");
	//encoding->print(proof.Y);  printf("\n");
	//encoding->print(proof.alphaV);  printf("\n");
	//encoding->print(proof.alphaW);  printf("\n");
	//encoding->print(proof.alphaY);  printf("\n");
	//encoding->print(proof.H);	  printf("\n");
	//encoding->print(proof.beta);	printf("\n");

  // Make some space for the IO
  proof.io = new FieldElt[circuit.numTrueInputs() + circuit.outputs.size()];
  proof.inputs = proof.io; 
  proof.outputs = proof.io + circuit.numTrueInputs();

	WireValues wireValuesIn;
	parseWireFile(circuit.field, config["input"].as<string>(), wireValuesIn);		

	// Apply the inputs
	for (WireVector::iterator iter = circuit.inputs.begin();
	     iter != circuit.inputs.end();
			 iter++) {
		WireValues::iterator lookup = wireValuesIn.find((*iter)->id);
		assert(lookup != wireValuesIn.end());	// If this triggers, we failed to find a value for an input wire!
		circuit.field->copy(lookup->second.elt, (*iter)->value);
	}
  
	for (int i = 0; i < circuit.numTrueInputs(); i++) {
		field->copy(circuit.inputs[i]->value, proof.inputs[i]);
	}

	WireValues wireValuesOut;
	parseWireFile(circuit.field, config["output"].as<string>(), wireValuesOut);		

	// Apply the outputs
	for (WireVector::iterator iter = circuit.outputs.begin();
	     iter != circuit.outputs.end();
			 iter++) {
		WireValues::iterator lookup = wireValuesOut.find((*iter)->id);
		assert(lookup != wireValuesOut.end());	// If this triggers, we failed to find a value for an output wire!
		circuit.field->copy(lookup->second.elt, (*iter)->value);
	}

	for (int i = 0; i < circuit.outputs.size(); i++) {
		field->copy(circuit.outputs[i]->value, proof.outputs[i]);
	}

	// Construct the QAP
	QAP qap(&circuit, encoding, config.count("pcache") > 0);	

	Keys* keys = new Keys(false, &qap, 1, 1, false);
	keys->pk = &pk;
	bool success = qap.verify(keys, &proof, config);

		
	delete encoding;
	//delete keys;

	if (success) {
		printf("Verification passed!\n");
	} else {
		printf("Verification failed!\n");
	}
}

/** Dispatch routine **/
void runQapTests(variables_map& config) {
	if (config.count("genkeys")) {
		runGenKeys(config);	
	} else if (config.count("dowork")) {
		runDoWork(config);	
	} else if (config.count("verify")) {
		runVerify(config);	
	} else if (config.count("server") || config.count("client")) {
		runNetworkTest(config);
	} else if (config.count("file")) {
		runPerfTest(config);	
	}	else if (config.count("test")) {
		runPolyCorrectnessTest();
		//randomPolyEvalInterpTest(50, config["trials"].as<int>());
		//runMultiExpTest(config["trials"].as<int>());
		//runSimpleTests(config);
		//runVerifyTest(config);
		runPaillierTests(config);
		//IOtest(config);
	} else if (config.count("matrix")) {
		runMatrixPerfTest(config);
	} else {
		printf("Unrecognized QAP option.  Try -h\n");
	}
}
