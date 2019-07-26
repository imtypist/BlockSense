#pragma once

#include "Types.h"
#include "Field.h"
#include "PolyTree.h"

class Field;
class PolyTree;

// Poly(x) = coefficients[i] * x^i

class Poly {
public:
	/*********  Instance Methods/Properties  ************/
	Poly(Field* field, int degree = -1);
	Poly(Field* field, int degree, int* coefficients);
	~Poly();

	int getDegree() { return degree; }
	void setDegree(int degree);

	// Reserve enough space to ensure we can represent a poly of degree specified
	void reserveSpace(int degree);

	// Set the coefficients to the array provided
	void setCoefficients(FieldElt* coeff, int degree);

	// Get a pointer to an array large enough to hold coeff for a degree d poly
	FieldElt* getCoefficientArray(int degree);

	bool equals(Poly& p);
	void print();
	
	FieldElt* coefficients;	

	/*********  Class Methods/Properties  ************/
	static void Initialize(Field* field);
	static void Cleanup();

	// result <- a + b
	static void add(Poly& a, Poly& b, Poly& result); 

	// result <- a - b
	static void sub(Poly& a, Poly& b, Poly& result);

	// result <- a * b
	static void mul(Poly& a, Poly& b, Poly& result);

	// Multiply a polynomial p by a constant c
	static void constMul(Poly& p, FieldElt &c, Poly& result);

	// Makes a polynomial monic.  Sets c to be coeff of x^maxdegree
	static void makeMonic(Poly& p, Poly& result, FieldElt &c);

	// Sanity check for the fast version
	static void mulSlow(Poly& a, Poly& b, Poly& result);
	
	// Create a polynomial that has all 0 coefficients
	static void zero(Poly& f, int degree);

	static bool isZero(Poly& p);

	// Reverse into a fresh polynomial
	static void reverse(Poly& f, Poly& fRev);
	
	// Reverse in place
	static void reverse(Poly& f);

	// Compute: f mod x^degree
	static void reduce(Poly& f, int degree);

	// Compute: result <- f^-1 mod x^degree
	static void invert(Poly& f, int degree, Poly& result);

	// Compute: result <- f / x^degree
	static void div(Poly& f, int degree, Poly& result);

	// Computes r <- f mod g
	static void mod(Poly& f, Poly& g, Poly& q, Poly& r);

	// Requires O(len * log^2 len) time
	static FieldElt* genLagrangeDenominators(Field* field, Poly& func, PolyTree* polyTree, FieldElt* x, int len);

	// Computes the coefficient representation of the Lagrange basis polynomial indicated by index
	static FieldElt* genLagrangeCoeffs(Field* field, FieldElt* evalPts, int len, int index);

	// Given a set of len points (x_i,y_i), find the degree n+1 polynomial that they correspond to
	static Poly* interpolate(Field* field, FieldElt* x, FieldElt* y, int len);

	static Poly* interpolateSlow(Field* field, FieldElt* x, FieldElt* y, int len);

	// Given a set of len points (x_i,y_i), find the degree n+1 polynomial that they correspond to
	// using the precalculated polynomial tree and Lagrange denominators
	// Requires O(len * log^2 len) time
	static Poly* interpolate(Field* field, FieldElt* y, int len, PolyTree* polyTree, FieldElt* denominators);	

	static void multiEval(Poly& f, FieldElt* x, int len, FieldElt* result);
	static void multiEval(Poly& f, FieldElt* x, int len, FieldElt* result, PolyTree* polys, int polyIndex, int resultIndex, int depth);

	static void interpolateRecursive(Field* field, FieldElt* s, PolyTree* polys, Poly& result, int len, int depth, int polyIndex);

	static void firstDerivative(Poly& poly);
	static void firstDerivative(Poly& poly, Poly& result);	

private:
	Field* field;
	int degree;
	int size;	// Number of coefficients we can hold	

	void init(Field* field, int degree);

	static modfft_info_t fftinfo;
	static bigctx_t BignumCtx;
	static bigctx_t* pbigctx;
};
