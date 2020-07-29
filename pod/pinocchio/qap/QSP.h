#pragma once

#include <vector>
#include "BoolCircuit.h"
#include "Field.h"
#include "Proof.h"
#include "Keys.h"
#include "Encoding.h"
#include "BoolPoly.h"

#include <boost/program_options.hpp>
using namespace boost::program_options;

class BoolCircuit;

typedef std::vector<WrappedElt> EltVector;

// Uncomment to compare slow poly interpolation to fast
//#define SLOW_POLY_INTERP

class QSP {
public:
	QSP(BoolCircuit* circuit, Encoding* encoding);
	~QSP();

	void strengthen();	// Convert this QSP into a strong QSP
	
	Keys* genKeys(variables_map& config);
	QspProof* doWork(PublicKey* pubKey, variables_map& config);	// Expects you to have evaluated the circuit associated with this QSP!
	bool verify(Keys* keys, QspProof* proof);

	int getSize() { return this->size; }
	int getDegree() { return this->degree; }

	void print();

	int genFreshRoots(int num, EltVector& roots);
	SparsePolynomial* newSparsePoly();
	SparsePolynomial* getSPtarget();		// Returns a pointer to V_0

	BoolCircuit* circuit;				// BoolCircuit this QSP verifies

	Field* field;						// Field this QSP is defined over

	EltVector vTargetRoots;  // t_v(x) = \prod(x-r), for all r in vTargetRoots	
	EltVector wTargetRoots;  // t_w(x) = \prod(x-r), for all r in wTargetRoots	
	EltVector wireCheckerTargetRoots;	// t'(x) = \prod(x-r), for all r in wireCheckerTargetRoots	

private:
	Encoding* encoding;			// Encoding we'll use when doing crypto with this QSPs

	// Each element of V corresponds to a v_k(x), 
	// and indicates the values in targetRoots at which v_k(x) is non-zero
	// (and the corresponding non-zero coefficient) 
	SparsePolyPtrVector V;			
	SparsePolyPtrVector W;	
	FieldElt* targetRoots;

	int degree;			// Max polynomial degree
	int size;			// Number of polynomials

	int polyIDctr;
	
	void printPolynomials(SparsePolyPtrVector& polys, int numPolys);

	// Lookup the polynomial associated with a particular gate or input value
	long long polyIndexForGate(long long gateID);
	long long polyIndexForInput(long long inputID);
	// Find the wire value corresponding to a particular polynomial
	bool wireValueForPolyIndex(long long index);

	void processGates();

	void duplicateVtoW();

	FieldElt* createLagrangeDenominators();
	FieldElt* evalLagrange(FieldElt& evaluationPoint, FieldElt* denominators);
	void evalSparsePoly(SparsePolynomial* poly, FieldElt* lagrange, FieldElt& result);

	// Sometimes, we end up with polynomials represented by arrays,
	// with each entry representing the value of the corresponding root
	void evalDensePoly(FieldElt* poly, int polySize, FieldElt* lagrange, FieldElt& result);

	Poly* interpolate(PublicKey* pk, FieldElt* poly);	
	Poly* getTarget(PublicKey* pk);

	// Take an evaluated circuit and compute an inner product between
	// the encoded values in the key and the coefficients of the circuit's wires
	// If midOnly is true, ignore the I/O wire values
	template <typename EncodedEltType>
	void applyBoolCircuitCoefficients(EncodedEltType* key, EncodedEltType& result, bool midOnly);

	// Helper function for applyBoolCircuitCoefficients
	template <typename EncodedEltType>
	void applyBoolWireValues(BoolWireVector& wires, EncodedEltType* key, EncodedEltType& result);

	// Apply a set of coefficients to a set of encoded values
	void applyCoefficients(variables_map& config, long numCoefficients, LEncodedElt* key, FieldElt* coefficients, LEncodedElt& result);

	// Combine a set of sparse polynomials into a single dense polynomial
	// The polynomials are in evaluation form (evaluated at targetRoots)
	FieldElt* computeDensePoly(SparsePolyPtrVector &polyVector);
	// Helper function
	void computeDensePolyPartial(BoolWireVector& wires, SparsePolyPtrVector &polyVector, FieldElt* result);
	// Helper for the helper
	void computeDensePolyPartialPoly(SparsePolynomial* poly, FieldElt* result);

	// Computes: result <- T(evaluationPoint)
	void evalTargetPoly(FieldElt& evaluationPoint, FieldElt* roots, int numRoots, FieldElt& result);

	// Computes the coefficient representation of the Lagrange basis polynomial indicated by index
	FieldElt* genLagrangeCoeffs(FieldElt* evalPts, int index);

	// Check whether base^exp == target
	template <typename EncodedEltType>
	bool checkConstEquality(EncodedEltType& base, FieldElt& exp, EncodedEltType& target);
};