#pragma once

#include <vector>
#include "Circuit.h"
#include "Field.h"
#include "Proof.h"
#include "Keys.h"
#include "Encoding.h"
#include "SparsePolynomial.h"

#include <boost/program_options.hpp>
using namespace boost::program_options;

class Circuit;
class GateMul2;
class Wire;

typedef vector<Wire*> WireVector;

// Uncomment to compare slow poly interpolation to fast
//#define SLOW_POLY_INTERP

typedef FieldElt* EltVector;

class QAP {
public:
	QAP(Circuit* circuit, Encoding* encoding, bool cachedBuild);
	~QAP();

	void strengthen();	// Convert this QAP into a strong QAP
	
	Keys* genKeys(variables_map& config);
	Proof* doWork(PublicKey* pubKey, variables_map& config);	// Expects you to have evaluated the circuit associated with this QAP!
	bool verify(Keys* keys, Proof* proof, variables_map& config);
	bool verify(Keys* keys, Proof* proof, variables_map& config, bool recordTiming);

	int getSize() { return this->size; }
	int getDegree() { return this->degree; }

	void print();

	Circuit* circuit;				// Circuit this QAP verifies

	// Each element of V corresponds to a v_k(x), 
	// and indicates the values in targetRoots at which v_k(x) is non-zero
	// (and the corresponding non-zero coefficient) 
	SparsePolynomial* V;			
	SparsePolynomial* W;	// Same as V, but for w_k(x)
	SparsePolynomial* Y;	// Same as V, but for y_k(x)

	// Lookup the polynomial associated with a particular mult gate or input value
	long long polyIndex(GateMul2* gate);
	long long polyIndex(Wire* wire);	// Only works for input wires!

private:
	Encoding* encoding;			// Encoding we'll use when doing crypto with this QAP
	Field* field;						// Field this QAP is defined over

	EltVector targetRoots;  // t(x) = \prod(x-r), for all r in targetRoots	

	int degree;			// Max polynomial degree
	int size;			// Number of polynomials

	int numTargetRoots;
	
	int genFreshRoots(int num);

	void printPolynomials(SparsePolynomial* polys, int numPolys);

	void processSubcircuit(GateMul2* circuit, long long gateID);
	void recurseTopological(long long rootID, Wire* wire, SparsePolynomial* polys, Modifier mod);

	// Find the wire value corresponding to a particular polynomial
	void wireValueForPolyIndex(long long index, FieldElt& result);

	FieldElt* createLagrangeDenominators();
	FieldElt* evalLagrange(FieldElt& evaluationPoint, FieldElt* denominators);
	void evalSparsePoly(SparsePolynomial* poly, FieldElt* lagrange, FieldElt& result);

	// Sometimes, we end up with polynomials represented by arrays,
	// with each entry representing the value of the corresponding root
	void evalDensePoly(FieldElt* poly, int polySize, FieldElt* lagrange, FieldElt& result);

	Poly* interpolate(PublicKey* pk, FieldElt* poly);	
	Poly* getTarget(PublicKey* pk);

#ifndef CHECK_NON_ZERO_IO
	// Take an evaluated circuit and setup the computation of an inner product between
	// the encoded values in the key and the coefficients of the circuit's wires
	// If midOnly is true, ignore the I/O wire values
	// Fills in the bases and exps arrays with pointers to the appropropriate values to be combined
	// Stop the timer passed in during pre-comp activities
	template <typename EncodedEltType>
	void applyCircuitCoefficients(variables_map& config, EncodedEltType* key, EncodedEltType& result, bool midOnly, Timer* timer);
	//void applyCircuitCoefficients(LEncodedElt* key, LEncodedElt& result, bool midOnly);
	//void applyCircuitCoefficients(REncodedElt* key, REncodedElt& result, bool midOnly);

	// Helper function for applyCircuitCoefficients
	template <typename EncodedEltType>
	void defineWireValues(EncodedEltType** bases, FieldElt** exps, int offset, WireVector& wires, EncodedEltType* key);
	//void applyWireValues(WireVector& wires, LEncodedElt* key, LEncodedElt& result);
	//void applyWireValues(WireVector& wires, REncodedElt* key, REncodedElt& result);
#else // defined CHECK_NON_ZERO_IO	
	// Take an evaluated circuit and compute an inner product between
	// the encoded values in the key and the coefficients of the circuit's wires
	// If midOnly is true, ignore the I/O wire values
	template <typename EncodedEltType>
	void applyCircuitCoefficients(variables_map& config, EncodedEltType* key, EncodedEltType& result, bool inclNonZeroWinputs, Timer* timer);
	
	// Helper function for applyCircuitCoefficients
	// result += key[k] * wire[k]
	template <typename EncodedEltType>
	void defineMidWireValues(EncodedEltType** bases, FieldElt** exps, int offset, WireVector& wires, EncodedEltType* key);
	// Helper function for applyCircuitCoefficients
	// result += key[k] * wire[k]
	template <typename EncodedEltType>
	int defineInputWireValues(EncodedEltType** bases, FieldElt** exps, int offset, WireVector& wires, EncodedEltType* key);

	template <typename EncodedEltType>
	void defineWitnessWireValues(EncodedEltType** bases, FieldElt** exps, int offset, WireVector& wires, EncodedEltType* key);

	template <typename EncodedEltType>
	void applyAllCircuitCoefficients(variables_map& config, EncodedEltType* key, EncodedEltType& result, Timer* timer);

	template <typename EncodedEltType>
	void defineAllWireValues(EncodedEltType** bases, FieldElt** exps, int offset, WireVector& wires, EncodedEltType* key);
#endif //#ifndef CHECK_NON_ZERO_IO

	// Apply a set of coefficients to a set of encoded values
	template <typename EncodedEltType>
	void applyCoefficients(MultiExpPrecompTable<EncodedEltType>& precompTable, long numCoefficients, FieldElt* coefficients, LEncodedElt& result);

	// Combine a set of sparse polynomials into a single dense polynomial
	// In both cases, the polynomials are in evaluation form (evaluated at targetRoots)
	// Compresses all of the polynomials in the QAP
	FieldElt* computeDensePoly(SparsePolynomial* polys);

	// Only compresses IO-related polys (including v_0(x) etc.)
	FieldElt* computeDensePolyIO(SparsePolynomial* polys);

	// Only compresses non-IO-related polys
	FieldElt* computeDensePolyMid(SparsePolynomial* polys);

	// Helper condenser function
	void condensePoly(SparsePolynomial* p, FieldElt& a_i, FieldElt* result);

	// Computes: result <- T(evaluationPoint)
	void evalTargetPoly(FieldElt& evaluationPoint, FieldElt& result);

	void applySparsePoly(SparsePolynomial* poly, LEncodedElt* lagrange, LEncodedElt& result);
	void applySparsePolysMid(SparsePolynomial* polys, LEncodedElt* lagrange, LEncodedElt* result);
	void applySparsePolys(SparsePolynomial* polys, LEncodedElt* lagrange, LEncodedElt* result);

	template <typename EncodedEltType>
	void computeDVio(Encoding* encoding, SparsePolynomial* polys, size_t len, size_t numInputs, FieldElt* secretKeyVals, FieldElt* proofVals, EncodedEltType& result);

	template <typename EncodedEltType>
	void computePVio(variables_map& config, Encoding* encoding, Circuit* circuit, SparsePolynomial* polys, size_t ioLen, size_t outStart, 
		               EncodedEltType* pubKeyVals, FieldElt* proofVals, EncodedEltType& result);

	// Check whether base^exp == target
	template <typename EncodedEltType>
	bool checkConstEquality(EncodedEltType& base, FieldElt& exp, EncodedEltType& target);
};