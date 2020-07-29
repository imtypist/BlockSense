
#define WIN32_LEAN_AND_MEAN  // Prevent conflicts in Window.h and WinSock.h
#include <assert.h>
#include "QSP.h"

#ifdef USE_OLD_PROTO

// We build up different sets of roots, but in the end, we combine them into one indexed array as:
// [vTargetRoots || wTargetRoots || wireCheckerTargetRoots]
QSP::QSP(BoolCircuit* circuit, Encoding* encoding) {
	this->encoding = encoding;
	this->field = encoding->getSrcField();
	this->circuit = circuit;
	this->polyIDctr = 0;

	assert(circuit->gates.size() > 1);	// Lagrange assumes more than 1 root, so we need more than one gate	
	
	V.reserve(circuit->gates.size());		// Give V a heads-up.  We'll want at least one (probably more) polys/gate
	newSparsePoly();	// Allocate a slot for v_0
	
	// Process each gate
	int gateID = 0;
	processGates();
	
	//print();

	// We just built V.  Use it as a model for W.	
	duplicateVtoW();

#ifdef VERBOSE
	printf("Prior to strengthening, the degree is %d\n", vTargetRoots.size());
#endif

	// Finally add the wire checker to strengthen the QSP
	this->strengthen();

	// Update the QSP's stats
	size = (int) (V.size() - 1); // V_0 doesn't count against the QSP's size
	degree = (int) (vTargetRoots.size() + wTargetRoots.size() + wireCheckerTargetRoots.size());

#ifdef VERBOSE
	printf("Constructed a QSP with size %d and degree %d\n", size, degree);
#endif

	// Finally, put the roots in a more accessible container	
	targetRoots = new FieldElt[degree];
	int rootIndex = 0;
	for (int i = 0; i < vTargetRoots.size(); i++) {	
		field->copy(vTargetRoots[i].elt, targetRoots[rootIndex++]);
	}
	for (int i = 0; i < wTargetRoots.size(); i++) {	
		field->copy(wTargetRoots[i].elt, targetRoots[rootIndex++]);
	}
	for (int i = 0; i < wireCheckerTargetRoots.size(); i++) {	
		field->copy(wireCheckerTargetRoots[i].elt, targetRoots[rootIndex++]);
	}
}

// W has the same sparse polynomials as V, but they're defined in terms of a separate set of roots.
void QSP::duplicateVtoW() {
	// Generate w's roots
	genFreshRoots((int)vTargetRoots.size(), wTargetRoots);

	// Duplicate the polynomials in terms of the new roots
	W.resize(V.size());
	for (int i = 0; i < V.size(); i++) {
		W[i] = new SparsePolynomial;
		W[i]->id = V[i]->id;

		for (V[i]->iterStart(); !V[i]->iterEnd(); V[i]->iterNext()) {	
			//	W draws from it's own roots, so we need to increment the ID far enough to get beyond V's roots
			W[i]->addNonZeroRoot(V[i]->curRootID() + (unsigned long) vTargetRoots.size(), *V[i]->curRootVal(), field);
		}
	}	
}


QSP::~QSP() {
	for (int i = 0; i < this->size+1; i++) {
		delete V[i];
		delete W[i];		
	}

	delete [] targetRoots;
}

int QSP::genFreshRoots(int num, EltVector& roots) {
	int oldSize = (int) roots.size();
	roots.resize(roots.size()+num);

	for (int i = 0; i < num; i++) {
		field->assignFreshElt(&roots[oldSize + i].elt);
	}

	return oldSize;
}

SparsePolynomial* QSP::newSparsePoly() {
	SparsePolynomial* ret = new SparsePolynomial();
	ret->id = polyIDctr++;
	V.resize(polyIDctr);
	V[polyIDctr-1] = ret;
	return ret;
}

// Returns a pointer to V_0
SparsePolynomial* QSP::getSPtarget() {
	return V.front();
}

void QSP::processGates() {
	for (BoolGateList::iterator iter = circuit->gates.begin();
	     iter != circuit->gates.end();
			 iter++) {
		(*iter)->generateSP(this);
	}
}

void QSP::printPolynomials(SparsePolyPtrVector& polys, int numPolys) {
	//for (int i = 0; i < numPolys; i++) {
	//	if (polys[i]->nonZeroRoots.size() > 0) {
	//		printf("%d\t", i);
	//		for (NonZeroRoots::iterator iter = polys[i]->nonZeroRoots.begin();
	//		     iter != polys[i]->nonZeroRoots.end();
	//		     iter++) {
	//			printf("(%lld, ", (*iter).first);
	//			field->print((*iter).second->value);
	//			printf("), ");	
	//		}
	//		printf("\n");
	//	} else {
	//		printf("%d\t All zero\n", i);			
	//	}
	//}
}

void QSP::print() {
	printf("Size: %d, degree: %d\n", size, degree);
	printf("Roots: {");

	for (int i = 0; i < vTargetRoots.size(); i++) {
		field->print(vTargetRoots[i].elt);
		if (i < vTargetRoots.size() - 1) {
			printf(", ");
		}
	}
	printf("}\n");

	printf("V index : V non-zero roots\n");
	printPolynomials(V, (int)V.size());
}

// Convert this QSP into a strong QSP (by adding a wire checker)
void QSP::strengthen() {
	FieldElt one;
	field->one(one);

#ifndef DEBUG_STRENGTHEN
	// For each wire in the circuit
	for (BoolWireVector::iterator witer = circuit->wires.begin();
	     witer != circuit->wires.end();
		 witer++) {
		int num0indices = (int) (*witer)->zero.size();
		int num1indices = (int) (*witer)->one.size();
		int maxIndices = max(num0indices, num1indices);	// = L_max
		int numNewRoots = 3 * maxIndices - 2;			// = L'

		// Create two new sets of roots of size numNewRoots
		int firstNewRootIndex = genFreshRoots(numNewRoots * 2, wireCheckerTargetRoots);

		int rootIndex = firstNewRootIndex;
		int rootIndexOffset = (int) (vTargetRoots.size() + wTargetRoots.size());	// These roots are ID'ed starting after the V and W roots

		// For k \in I_0
		for (SparsePolyPtrVector::iterator iter = (*witer)->zero.begin();
			 iter != (*witer)->zero.end();
			 iter++) {
			V[(*iter)->id]->addNonZeroRoot(numNewRoots + rootIndex + rootIndexOffset, one, field); // v_k(r^(1)_k) = 1
			W[(*iter)->id]->addNonZeroRoot(rootIndex + rootIndexOffset, one, field);	// w_k(r^(0)_k) = 1

			rootIndex++;
		}

		rootIndex = firstNewRootIndex;
		// For k \in I_1
		for (SparsePolyPtrVector::iterator iter = (*witer)->one.begin();
			 iter != (*witer)->one.end();
			 iter++) {
			V[(*iter)->id]->addNonZeroRoot(rootIndex + rootIndexOffset, one, field);  // v_k(r^(0)_k) = 1
			W[(*iter)->id]->addNonZeroRoot(numNewRoots + rootIndex + rootIndexOffset, one, field); // w_k(r^(1)_k) = 1

			rootIndex++;
		}		
	}

#endif

	// We also need to update v_0 and w_0 when we combine everything together

	// v_0(x) = 1 mod t^{W}(x)
	for (int i = 0; i < wTargetRoots.size(); i++) {
		V[0]->addNonZeroRoot((int) (i + vTargetRoots.size()), one, field);
	}

	// w_0(x) = 1 mod t^{V}(x)
	for (int i = 0; i < vTargetRoots.size(); i++) {
		W[0]->addNonZeroRoot(i, one, field);
	}
}

FieldElt* QSP::createLagrangeDenominators() {
	FieldElt* factorial = new FieldElt[this->degree];
	FieldElt* denominators = new FieldElt[this->degree];

	// We exploit the fact that root[k] = root[k-1] + 1
	// which means that denom_i = \prod_{k=1}^{i-1} k \prod_{k=1}^{d-i} (-k),
	// where the second term = (-1)^{d-1} \prod_{k=1}^{d-i} k
	// Thus, we compute 1!, 2!, 3!, ..., degree!, and store it s.t. factorial[i] = (i+1)!
	FieldElt ctr, zero, one;
	field->one(factorial[0]);
	field->one(ctr);
	field->zero(zero);
	field->one(one);
	for (int k = 1; k < this->degree; k++) { // For each r_k
		field->add(ctr, one, ctr);
		field->mul(factorial[k-1], ctr, factorial[k]);
	}

	for (int i = 0; i < this->degree; i++) {	// For each L_i
		// Calculate the denominator = (k-1)! * (d-k)! * (-1)^{d-1}
		int k = i + 1;	// 1-based index
		int dMinusK = this->degree - k - 1 >= 0 ? this->degree - k - 1 : 0;
		int kFact = k - 1 - 1 > 0 ? k - 2 : 0;
		field->mul(factorial[kFact], factorial[dMinusK], denominators[i]);	// -1 for 0-based arrawy
		if ((this->degree - k) % 2 == 1) { // Need a -1 term
			field->sub(zero, denominators[i], denominators[i]);
		}
	}

	delete [] factorial;

	return denominators;
}

// For each root r_i, compute L_i(val),
// where L_i(x) = \prod_k (x - r_k)/(r_i-r_k) = 1 if x = r_i, 0 if x = r_j for j <> i
FieldElt* QSP::evalLagrange(FieldElt& evaluationPoint, FieldElt* denominators) {
	FieldElt* L = new FieldElt[this->degree];
	FieldElt* diff = new FieldElt[this->degree];
	
	// First, calculate the "master" numerator, i.e., \prod_k (x - r_k)
	FieldElt superNumerator;
	field->one(superNumerator);

	for (int k = 0; k < this->degree; k++) { // For each r_k
		field->sub(evaluationPoint, targetRoots[k], diff[k]);
		field->mul(superNumerator, diff[k], superNumerator);	// superNumerator *= (evalPt - root[k])
	}

	// Finally, calculate the individual Lagrange polynomial values
	for (int i = 0; i < this->degree; i++) {	// For each L_i
		// Remove (x-r_k) if k == i
		FieldElt numerator;
		field->div(superNumerator, diff[i], numerator);	

		// Do the final calculation
		field->div(numerator, denominators[i], L[i]);
	}

	delete [] diff;
	
	return L;
}

// We store the polynomials in a compressed representation, based on their values at certain roots
// This means, we're implicitly representing them as pairs of (i, c_i), such that
// p(x) = \sum c_i L_i(x), where L_i is the Lagrange polynomial associated with the i-th root
void QSP::evalSparsePoly(SparsePolynomial* poly, FieldElt* lagrange, FieldElt& result) {
	field->zero(result);

	FieldElt tmp;
	for (poly->iterStart(); !poly->iterEnd(); poly->iterNext()) {
		field->mul(lagrange[poly->curRootID()], *poly->curRootVal(), tmp);
		field->add(result, tmp, result);	// result += L_i * c_i
	}
}

// Sometimes, we end up with polynomials represented by arrays,
// with each entry representing the value of the corresponding root
void QSP::evalDensePoly(FieldElt* poly, int polySize, FieldElt* lagrange, FieldElt& result) {
	field->zero(result);

	FieldElt tmp;
	for (int i = 0; i < polySize; i++) {
		field->mul(lagrange[i], poly[i], tmp);
		field->add(result, tmp, result);
	}
}

// Computes: result <- T(evaluationPoint)
void QSP::evalTargetPoly(FieldElt& evaluationPoint, FieldElt* roots, int numRoots, FieldElt& result) {
	field->one(result);

	FieldElt tmp;
	for (int i = 0; i < numRoots; i++) {
		field->sub(evaluationPoint, roots[i], tmp);
		field->mul(result, tmp, result);	// result += evalPt * root[i]		
	}
}

// SLOW VERSION:
// Computes the coefficient representation of the Lagrange basis polynomial indicated by index
// Observe that: L_index(x) = \prod_{i != index} 1/(s_index - s_i) * \prod_{i != index} (x - s_i)
// The left term has no dependence on x, so we can compute that separately and then apply to the
// coefficients that come from the right term
FieldElt* QSP::genLagrangeCoeffs(FieldElt* evalPts, int index) {
	FieldElt zero, tmp;
	field->zero(zero);
	FieldElt* coefficients = new FieldElt[this->degree];
	field->zero(coefficients, degree);
	
	field->one(coefficients[0]);	
	// Compute (x-evalPt[0])(x-evalPt[1]) ... (x-evalPt[d]), one term at a time
	int rootCount = 0;	// Separate from i, since we skip one of the roots
	for (int i = 0; i < this->degree; i++) { // For each root
		if (i == index) {
			continue;	
		} 
		rootCount++;
		// Current coefficient = prevValShiftedOne - root*prevVal
		for (int j = rootCount; j >= 0; j--) {
			if (j == 0) {
				field->sub(zero, evalPts[i], tmp);	// Multiply by -1*r_i
				field->mul(tmp, coefficients[0], coefficients[0]);
			} else {
				field->mul(evalPts[i], coefficients[j], coefficients[j]);
				field->sub(coefficients[j-1], coefficients[j], coefficients[j]);
			}
		}
	}

	// Compute the divisor Prod_{i <> j} (x_i - x_j)
	FieldElt prod;
	field->one(prod);
	for (int i = 0; i < this->degree; i++) { // For each root
		if (i == index) {
			continue;	
		} 
		field->sub(evalPts[index], evalPts[i], tmp);
		field->mul(tmp, prod, prod);		
	}

	// Apply the divisor to all of the coefficients
	for (int i = 0; i < this->degree; i++) { // For each root
		field->div(coefficients[i], prod, coefficients[i]);	
	}

	return coefficients;
}

Keys* QSP::genKeys(variables_map& config) {
	Keys* keys = new Keys(config.count("dv") != 0, this, this->degree+1, this->size+1, config.count("nizk") != 0);
	PublicKey* pk = keys->pk;
	SecretKey* sk = keys->sk;

	Timer* randTimer = timers.newTimer("GenRandom", "KeyGen");
	randTimer->start();

	// Select a secret evaluation point (s)
	FieldElt s;
	field->assignRandomElt(&s);
	
	// Select secret multiples to ensure extractability
	FieldElt alpha, betaV, betaW, gamma;
	field->assignRandomElt(&alpha);
	field->assignRandomElt(&betaV);
	field->assignRandomElt(&betaW);
	field->assignRandomElt(&gamma);

	randTimer->stop();
	
	// Precalculate powers of g to speed up all of the exponentiations below
	int numEncodings =  2*degree   // s^i and alpha*s^i
										+ 3*size*2;  // (V, alphaV, betaV) * 2 for W
	TIME(encoding->precomputePowersFast(numEncodings, config["mem"].as<int>(), config.count("fill") != 0), "PrePwrs", "KeyGen");

	if (config.count("dv")) {
		field->copy(alpha, sk->alpha);
		field->copy(betaV, sk->betaV);
		field->copy(betaW, sk->betaW);
		field->copy(gamma, sk->gamma);
	} else {
		Timer* eTimer = timers.newTimer("EncodeKEA", "KeyGen");	// Encode the knowledge of exponent vals, \alpha, \beta, etc.
		eTimer->start();

		FieldElt betaVgamma, betaWgamma;
		field->mul(betaV, gamma, betaVgamma);
		field->mul(betaW, gamma, betaWgamma);
		encoding->encode(alpha, *pk->alphaL);
		encoding->encode(alpha, *pk->alphaR);
		encoding->encode(gamma, *pk->gammaR);
		encoding->encode(betaVgamma, *pk->betaVgamma);
		encoding->encode(betaWgamma, *pk->betaWgamma);	

		eTimer->stop();
	}
	
	// Create encodings of s^i and alpha*s^i for i \in [0, degree]
	FieldElt sPower;
	field->one(sPower);	// = s^0
	FieldElt alphaSpower;
	field->copy(alpha, alphaSpower); // = alpha*s^0

	Timer* sTimer = timers.newTimer("PwrsOfS", "KeyGen");
	sTimer->start();
	for (int i = 0; i < this->degree + 1; i++) {		
		encoding->encode(sPower, pk->powers[i]);
		encoding->encode(alphaSpower, pk->alphaPowers[i]);
		field->mul(sPower, s, sPower);
		field->mul(alphaSpower, s, alphaSpower);
	}
	sTimer->stop();

	// Evaluate each polynomial at s
	Timer* evalTimer = timers.newTimer("EvalAtS", "KeyGen");
	evalTimer->start();

	FieldElt* vAtS = new FieldElt[this->size+1];
	FieldElt* wAtS = new FieldElt[this->size+1];

	// To aid the evaluation of the QSP's polynomials,
	// we precalculate each root's corresponding Lagrange polynomial at s
	// This goes into the public key too, since it aids in the calculation of h(x)
	pk->denominators = createLagrangeDenominators();	
	FieldElt* lagrange = evalLagrange(s, pk->denominators);		
	for (int k = 0; k < this->size+1; k++) {
		evalSparsePoly(V[k], lagrange, vAtS[k]);
		evalSparsePoly(W[k], lagrange, wAtS[k]);	// Same sparse configuration, but different roots
	}
	delete [] lagrange;
	evalTimer->stop();

	if (config.count("dv")) {
		// Secret key holds v_0, v_in, v_out; all evaluated at s 
		Timer* copyTimer = timers.newTimer("CopyVio", "KeyGen");
		copyTimer->start();

		sk->vVals.insert(IndexedElt(0, *((WrappedElt*)(&vAtS[0]))));  // v_0

		// For each I/O wire
		for (BoolWireVector::iterator witer = circuit->ioWires.begin();
				 witer != circuit->ioWires.end();
				 witer++) {
			// For each poly representing the wire with value 0
			for (SparsePolyPtrVector::iterator iter = (*witer)->zero.begin();
					 iter != (*witer)->zero.end();
					 iter++) {			
				sk->vVals.insert(IndexedElt((*iter)->id, *((WrappedElt*)(&vAtS[(*iter)->id]))));
			}
			// For each poly representing the wire with value 1
			for (SparsePolyPtrVector::iterator iter = (*witer)->one.begin();
					 iter != (*witer)->one.end();
					 iter++) {
				sk->vVals.insert(IndexedElt((*iter)->id, *((WrappedElt*)(&vAtS[(*iter)->id]))));
			}
		}

		copyTimer->start();
	}

	// Encode the evaluated polynomials
	Timer* encodeTimer = timers.newTimer("EncodePolys", "KeyGen");	
	Timer* baseEncodeTimer = timers.newTimer("BaseEncoding", "EncodePolys");
	Timer* twistEncodeTimer = timers.newTimer("TwistEncoding", "EncodePolys");
	encodeTimer->start();
	for (int k = 0; k < this->size+1; k++) {
		baseEncodeTimer->start();
		encoding->encode(vAtS[k], pk->V[k]);				
		baseEncodeTimer->stop();

		twistEncodeTimer->start();
		encoding->encode(wAtS[k], pk->W[k]);	
		twistEncodeTimer->stop();
	}

	// Calculate \alpha times the evaluated polynomials
	for (int k = 0; k < this->size+1; k++) {
		field->mul(alpha, vAtS[k], vAtS[k]);
		field->mul(alpha, wAtS[k], wAtS[k]);
	}

	// Encode \alpha times the evaluated polynomials
	for (int k = 0; k < this->size+1; k++) {
		baseEncodeTimer->start();
		encoding->encode(vAtS[k], pk->alphaV[k]);				
		baseEncodeTimer->stop();

		twistEncodeTimer->start();
		encoding->encode(wAtS[k], pk->alphaW[k]);
		twistEncodeTimer->stop();
	}

	// Calculate \beta times the evaluated polynomials
	FieldElt betaVOverAlpha, betaWOverAlpha;
	field->div(betaV, alpha, betaVOverAlpha);
	field->div(betaW, alpha, betaWOverAlpha);

	for (int k = 0; k < this->size+1; k++) {
		field->mul(betaVOverAlpha, vAtS[k], vAtS[k]);
		field->mul(betaWOverAlpha, wAtS[k], wAtS[k]);		
	}

	// Encode \beta times the evaluated polynomials
	for (int k = 0; k < this->size+1; k++) {
		baseEncodeTimer->start();
		encoding->encode(vAtS[k], pk->betaV[k]);	
		baseEncodeTimer->stop();

		twistEncodeTimer->start();
		encoding->encode(wAtS[k], pk->betaW[k]);	
		twistEncodeTimer->stop();
	}
	encodeTimer->stop();

	// Precompute the polynomial tree needed to quickly compute h(x)
	TIME(pk->polyTree = new PolyTree(field, targetRoots, degree), "PrePolyTree", "KeyGen");
	TIME(evalTargetPoly(s, targetRoots, degree, sk->target), "PreTatS", "KeyGen");
	
	if (!config.count("dv")) {
		TIME(encoding->encode(sk->target, *pk->RtAtS), "EncodeTatS", "KeyGen");

		if (config.count("nizk")) {
			Timer* nizkTimer = timers.newTimer("EncodeNIZK", "KeyGen");	// Encode the extra values we need for NIZKs
			nizkTimer->start();

			pk->LtAtS = (LEncodedElt*) new LEncodedElt;
			pk->LalphaTatS = (LEncodedElt*) new LEncodedElt;
			pk->RalphaTatS = (REncodedElt*) new REncodedElt;
			pk->LbetaTatS = (LEncodedElt*) new LEncodedElt;

			encoding->encode(sk->target, *pk->LtAtS);

			FieldElt tmp;
			field->mul(alpha, sk->target, tmp);
			encoding->encode(tmp, *pk->LalphaTatS);
			encoding->encode(tmp, *pk->RalphaTatS);
			field->mul(betaV, sk->target, tmp);
			encoding->encode(tmp, *pk->LbetaTatS);	

			nizkTimer->stop();
		}
	}

	delete [] vAtS;
	delete [] wAtS;

	return keys;
}

// Apply a set of coefficients to a set of encoded values
void QSP::applyCoefficients(variables_map& config, long numCoefficients, LEncodedElt* key, FieldElt* coefficients, LEncodedElt& result) {
	MultiExpPrecompTable<LEncodedElt> precompTable;

	TIME(encoding->precomputeMultiPowers(precompTable, key, (int)numCoefficients, 1, config["mem"].as<int>(), config.count("fill") != 0), "PreCompMulti", "OneTimeWork");
	encoding->multiMul(precompTable, coefficients, (int)numCoefficients, result);
	encoding->deleteMultiTables(precompTable);

	/*
	multi_exponent_t* multExp = new multi_exponent_t[numCoefficients];

	// Describe the calculation we want to perform
	for (int i = 0; i < numCoefficients; i++) {
		multExp[i].pbase = key[i];
		multExp[i].pexponent = coefficients[i];

		multExp[i].lng_bits = sizeof(FieldElt) * 8 -2;
		multExp[i].lng_pexp_alloc =	(int) ceil((double)multExp[i].lng_bits / (sizeof(digit_t) * 8));		
		multExp[i].offset_bits = 0; 
	}

	// Do it!
	encoding->multiMul(multExp, (int)numCoefficients, result);

	delete [] multExp;
	*/
}

// Helper function for applyBoolCircuitCoefficients
// result += key[k] * wire[k]
template <typename EncodedEltType>
void QSP::applyBoolWireValues(BoolWireVector& wires, EncodedEltType* key, EncodedEltType& result) {
	encoding->zero(result);

	for (long i = 0; i < (long)wires.size(); i++) {
		for (SparsePolyPtrVector::iterator iter = wires[i]->eval.begin();
		     iter != wires[i]->eval.end();
				 iter++) {
			encoding->add(result, key[(*iter)->id] , result);
		}		
	}
}

// Take an evaluated circuit and compute an inner product between
// the encoded values in the key and the coefficients of the circuit's wires
// If midOnly is true, ignore the I/O wire values
template <typename EncodedEltType>
void QSP::applyBoolCircuitCoefficients(EncodedEltType* key, EncodedEltType& result, bool midOnly) {
	applyBoolWireValues(circuit->intermediates, key, result);

	if (!midOnly) {
		EncodedEltType tmp;

		applyBoolWireValues(circuit->inputs, key, tmp);
		encoding->add(result, tmp, result);

		applyBoolWireValues(circuit->outputs, key, tmp);
		encoding->add(result, tmp, result);
	}
}


void QSP::computeDensePolyPartialPoly(SparsePolynomial* poly, FieldElt* result) {
	// For each non-zero root value of the poly
	for (poly->iterStart(); !poly->iterEnd(); poly->iterNext()) {
		field->add(result[poly->curRootID()], *poly->curRootVal(), result[poly->curRootID()]);	
	}
}

void QSP::computeDensePolyPartial(BoolWireVector& wires, SparsePolyPtrVector &polyVector, FieldElt* result) {
	// For each wire:
	for (BoolWireVector::iterator witer = wires.begin();
	     witer != wires.end();
			 witer++) {
		// For each poly that matches the wire's current value
		for (SparsePolyPtrVector::iterator piter = (*witer)->eval.begin();
		     piter != (*witer)->eval.end();
		     piter++) {
			computeDensePolyPartialPoly(polyVector[(*piter)->id], result);
		}
	}
}

// Combine a set of sparse polynomials into a single dense polynomial
// The polynomials are in evaluation form (evaluated at targetRoots)
FieldElt* QSP::computeDensePoly(SparsePolyPtrVector &polyVector) {
	FieldElt* result = new FieldElt[degree];
	field->zero(result, degree);	

	// Include v_0/w_0
	computeDensePolyPartialPoly(polyVector[0], result);

	// Include polynomials for all of the wires in the circuit, based on their evaluated value
	computeDensePolyPartial(circuit->wires, polyVector, result);
	
	return result;	
}


Poly* QSP::interpolate(PublicKey* pk, FieldElt* poly) {
	return Poly::interpolate(field, poly, degree, pk->polyTree, pk->denominators);
}

Poly* QSP::getTarget(PublicKey* pk) {
	return pk->polyTree->polys[pk->polyTree->height-1][0];	// Root of the tree is product of all the (x-r_i) terms
}

QspProof* QSP::doWork(PublicKey* pubKey, variables_map& config) {
	QspProof* proof = new QspProof((int)circuit->inputs.size(), (int)circuit->outputs.size());

	// Copy over the inputs and outputs
	for (int i = 0; i < (int) circuit->inputs.size(); i++) {
		proof->inputs[i] = circuit->inputs[i]->value;
	}

	for (int i = 0; i < circuit->outputs.size(); i++) {
		proof->outputs[i] = circuit->outputs[i]->value;
	}

	Timer* fastPolyInterpTimer = timers.newTimer("FastInterp", "DoWork");
	fastPolyInterpTimer->start();
	// Calculate v(x) and w(x) in terms of evaluations at targetRoots
	FieldElt* denseV = computeDensePoly(V);
	FieldElt* denseW = computeDensePoly(W);
	
	// Now interpolate them to convert to coefficient representation
	// (i.e., find v_i, s.t. v(x) = \sum v_i * x^i)
	Poly* V = interpolate(pubKey, denseV);
	Poly* W = interpolate(pubKey, denseW);

	// Free up as much memory as we can before doing the large multiplication
	delete [] denseV;
	delete [] denseW;
	pubKey->polyTree->deleteAllButRoot();
	delete [] pubKey->denominators;
	pubKey->denominators = NULL;

	// Now calculate Ht <- V*W
	Poly Ht(field);
	Poly::mul(*V, *W, Ht);

	// Finally calculate H <- Ht/t
	Poly* T = getTarget(pubKey);
	Poly H(field), r(field), polyZero(field);
	Poly::zero(polyZero, 1);
	Poly::mod(Ht, *T, H, r);
	assert(r.equals(polyZero));

	delete pubKey->polyTree;	
	pubKey->polyTree = NULL;
	
	fastPolyInterpTimer->stop();

	Timer* applyTimer = timers.newTimer("ApplyCoeff", "DoWork");
	Timer* baseApplyTimer = timers.newTimer("BaseApply", "EncodePolys");
	Timer* twistApplyTimer = timers.newTimer("TwistApply", "EncodePolys");
	applyTimer->start();
	baseApplyTimer->start();
	applyBoolCircuitCoefficients(pubKey->V, proof->V, true);		
	baseApplyTimer->stop();
	twistApplyTimer->start();
	applyBoolCircuitCoefficients(pubKey->W, proof->W, false); 
	twistApplyTimer->stop();

	baseApplyTimer->start();
	applyBoolCircuitCoefficients(pubKey->alphaV, proof->alphaV, true);	
	baseApplyTimer->stop();
	twistApplyTimer->start();
	applyBoolCircuitCoefficients(pubKey->alphaW, proof->alphaW, false);
	twistApplyTimer->stop();

	// Calculate Encoding(\beta_v * v_mid(s) + \beta_w * w(s))
	baseApplyTimer->start();
	LEncodedElt tmpV, tmpW;
	applyBoolCircuitCoefficients(pubKey->betaV, tmpV, true);
	applyBoolCircuitCoefficients(pubKey->betaW, tmpW, false);
	encoding->add(tmpV, tmpW, proof->beta);
	baseApplyTimer->stop();	
	applyTimer->stop();
	
	Timer* nizkTimer = NULL;
	FieldElt deltaV, deltaW, deltaVW;
	
	if (config.count("nizk")) {	
		nizkTimer = timers.newTimer("NIZKcalc", "DoWork");
		nizkTimer->start();

		// Rerandomize to get h'(x) <- h(x) + W(x)*deltaV + V(x)*deltaW + deltaV*deltaW*t(x)		
		field->assignRandomElt(&deltaV);
		field->assignRandomElt(&deltaW);
		field->mul(deltaV, deltaW, deltaVW);

		Poly tmp(field);
		Poly::constMul(*W, deltaV, tmp);
		Poly::add(H, tmp, H);	// h <- h(x) + W(x)*deltaV
		Poly::constMul(*V, deltaW, tmp);
		Poly::add(H, tmp, H);	// h += V(x)*deltaW
		Poly::constMul(*T, deltaVW, tmp);
		Poly::add(H, tmp, H);	// h += deltaV*deltaW*t(x)	

		nizkTimer->stop();
	}

	// Apply the hCoefficients to the power terms (s^i) we received in the key,
	// to get h(s) in the exponent	
	applyTimer->start();
	baseApplyTimer->start();
	applyCoefficients(config, H.getDegree() + 1, pubKey->powers,      H.coefficients, proof->H     );
	applyCoefficients(config, H.getDegree() + 1, pubKey->alphaPowers, H.coefficients, proof->alphaH);
	baseApplyTimer->stop();
	applyTimer->stop();

#ifdef SLOW_POLY_INTERP 
	// Combine v(x), w(x), and y(x) in terms of their evaluations at targetRoots
	// denseProd(x) = V(x) * W(x) 
	/* Note: This doesn't work.  We've defined V(x) * W(x) such that it equals 0 mod t(x),
	   which means that V(x) * W(x) contains all of the roots that t(x) does,
	   meaning that V(r_i) * W(r_i) = 0, for all r_i
	FieldElt* denseProd = new FieldElt[degree];	
	for (int i = 0; i < degree; i++) {
		field->mul(denseV[i], denseW[i], denseProd[i]);
	}
	*/

	// Compute h(x) = [V(x) * W(x)]/t(x).  
	// Approach: Compute h(evalPt_i) for degree fresh values evalPt_i
	// Interpolate.  Use Lagrange for now.  Eventually, we may want to swap to 
	// a Fourier transform approach (without the n-th root of unity bit, 
	// since we're in a prime-order field)
	// TODO: Should choose these and precompute v_0(x), w_0(x), y_0(x), and t(x)

	Timer* slowPolyInterpTimer = timers.newTimer("SlowPolyInterp", "DoWork");
	slowPolyInterpTimer->start();

	// Choose fresh evaluation points
	FieldElt* evalPts = new FieldElt[degree];
	for (int i = 0; i < pubKey->qap->degree; i++) {
		field->assignFreshElt(&evalPts[i]);
	}

	// Compute h(x) = (v(x)w(x) - y(x)) / t(x) at each of the new evaluation points
	FieldElt* denseH = new FieldElt[degree];
	FieldElt zero;
	field->zero(zero);
	FieldElt* denominators = createLagrangeDenominators();
	for (int i = 0; i < degree; i++) {
		FieldElt v, w, y, t, tmp;
		FieldElt* lagrange = evalLagrange(evalPts[i], denominators);

		evalDensePoly(denseV, degree, lagrange, v);
		evalDensePoly(denseW, degree, lagrange, w);
		evalTargetPoly(evalPts[i], t);

		field->mul(v, w, tmp);
		field->sub(tmp, y, tmp);
		if (field->equal(t, zero)) {
			field->zero(denseH[i]);
		} else {
			field->div(tmp, t, denseH[i]);
		}

		delete [] lagrange;
	}
	delete [] denominators;

	// Do the interpolation by computing the new Lagrange coefficients
	// h(x) = sum_i h_i * L_i(x) = sum_k hCoefficients[k] * x^k
	FieldElt* hCoefficients = new FieldElt[degree];
	field->zero(hCoefficients, degree);
	
	for (int i = 0; i < degree; i++) {
		// Compute the coefficient representation of L_i(x)
		FieldElt* lagrangeCoeff = genLagrangeCoeffs(evalPts, i);

		// Apply them to the running tally of coefficient values
		for (int j = 0; j < degree; j++) {
			FieldElt tmp;
			field->mul(lagrangeCoeff[j], denseH[i], tmp);
			field->add(hCoefficients[j], tmp, hCoefficients[j]);	// hCoeff[j] += denseH[i]*lCoeff[j]
		}

		delete [] lagrangeCoeff;
	}

	slowPolyInterpTimer->stop();

	// Did we get the same coefficients from both methods?
#ifdef DEBUG
	for (int i = 0; i < degree-1; i++) {
		assert(field->equal(hCoefficients[i], H.coefficients[i]));
	}
#endif // DEBUG

	// Apply the hCoefficients to the power terms (s^i) we received in the key,
	// to get h(s) in the exponent
	//applyCoefficients(degree, pubKey->powers,      hCoefficients, proof->H     );
	//applyCoefficients(degree, pubKey->alphaPowers, hCoefficients, proof->alphaH);

	delete [] denseH;
	delete [] evalPts;
	delete [] hCoefficients;
#endif // SLOW_POLY_INTERP

	delete V;
	delete W;

	if (config.count("nizk")) {
		nizkTimer->start();

		// Rerandomize V(x) and W(x)
		LEncodedElt deltaVt;
		LEncodedElt deltaWt;
		REncodedElt rdeltaWt;
		encoding->mul(*pubKey->LtAtS, deltaV, deltaVt);
		encoding->mul(*pubKey->LtAtS, deltaW, deltaWt);
		encoding->mul(*pubKey->RtAtS, deltaW, rdeltaWt);
		encoding->add(proof->V, deltaVt, proof->V);
		encoding->add(proof->W, rdeltaWt, proof->W);

		// Rerandomize \alpha*V(x) and \alpha*W(x)
		encoding->mul(*pubKey->LalphaTatS, deltaV, deltaVt);
		encoding->mul(*pubKey->LalphaTatS, deltaW, deltaWt);
		encoding->add(proof->alphaV, deltaVt, proof->alphaV);
		encoding->add(proof->alphaW, rdeltaWt, proof->alphaW);

		// Rerandomize \beta_v*V(x) and \beta_w*W(x)
		encoding->mul(*pubKey->LbetaTatS, deltaV, deltaVt);
		encoding->mul(*pubKey->LbetaTatS, deltaW, deltaWt);
		encoding->add(proof->beta, deltaVt, proof->beta);
		encoding->add(proof->beta, deltaWt, proof->beta);
		
		nizkTimer->stop();
	}

	return proof;
}

// Check whether base^exp == target
template <typename EncodedEltType>
bool QSP::checkConstEquality(EncodedEltType& base, FieldElt& exp, EncodedEltType& target) {
	EncodedEltType expResult;
	encoding->mul(base, exp, expResult);
	return encoding->equals(expResult, target);
}

bool QSP::verify(Keys* keys, QspProof* proof) {
	PublicKey* pk = keys->pk;
	SecretKey* sk = keys->sk;

	Timer* ioTimer = timers.newTimer("IO", "Verify");
	ioTimer->start();

	// DV means we can operate directly on the field values
	// Public means we have to operate inside the encoding
	LEncodedElt encodedV;
	if (sk->designatedVerifier) {
		// Compute V terms for input and output	
		FieldElt fieldV;  
		field->copy(sk->vVals.find(0)->second.elt, fieldV);	// Start with v_0

		// For each I/O wire:
		for (BoolWireVector::iterator witer = circuit->ioWires.begin();
				 witer != circuit->ioWires.end();
				 witer++) {
			// For each poly that matches the wire's current value
			for (SparsePolyPtrVector::iterator piter = (*witer)->eval.begin();
					 piter != (*witer)->eval.end();
					 piter++) {
				field->add(fieldV, sk->vVals.find((*piter)->id)->second.elt, fieldV);
			}
		}

		encoding->encode(fieldV, encodedV);
	} else {
		// Compute V terms for input, output, and v_0
		
		// v_0
		encoding->copy(pk->V[0], encodedV);		

		// I/O				
		for (BoolWireVector::iterator witer = circuit->ioWires.begin();
				 witer != circuit->ioWires.end();
				 witer++) {
			// For each poly that matches the wire's current value
			for (SparsePolyPtrVector::iterator piter = (*witer)->eval.begin();
					 piter != (*witer)->eval.end();
					 piter++) {
				encoding->add(encodedV, pk->V[(*piter)->id], encodedV);
			}
		}
	}		
	ioTimer->stop();

	Timer* checkTimer = timers.newTimer("Checks", "Verify");
	checkTimer->start();

	// Combine I/O terms with the V term from the proof
	encoding->add(encodedV, proof->V, encodedV);
	
	// W(x) = w_0(x) + proof->W
	REncodedElt encodedW;
	encoding->add(pk->W[0], proof->W, encodedW);

	// Check whether V*W = H * T
	bool quadraticCheck;
	FieldElt one;
	field->one(one);
	REncodedElt rEncodedOne;
	encoding->encode(one, rEncodedOne);	// Enc(1)

	if (sk->designatedVerifier) {
		LEncodedElt hTimesT;
		encoding->mul(proof->H, sk->target, hTimesT);	// Enc(H(s)*T(s))

		quadraticCheck = encoding->mulEquals(encodedV, encodedW, hTimesT, rEncodedOne);
	} else {
		quadraticCheck = encoding->mulEquals(encodedV, encodedW, proof->H, *(pk->RtAtS));
	}
#ifdef VERBOSE
	if (quadraticCheck) { 
		printf("\tPassed quadratic check!\n");
	} else {
		printf("\tFailed quadratic check!\n");
	}
#endif // VERBOSE

//	return encoding->mulEquals(base, encodedExp, target, encodedOne);

	// Check that the base terms match the alpha terms
	bool alphaCheck = true;

	if (sk->designatedVerifier) {
		alphaCheck = alphaCheck && checkConstEquality(proof->V, sk->alpha, proof->alphaV);
		alphaCheck = alphaCheck && checkConstEquality(proof->W, sk->alpha, proof->alphaW);
		alphaCheck = alphaCheck && checkConstEquality(proof->H, sk->alpha, proof->alphaH);
	} else {
		LEncodedElt lEncodedOne;
		encoding->encode(one, lEncodedOne);	// Enc(1)

		alphaCheck = alphaCheck && encoding->mulEquals(proof->V, *(pk->alphaR), proof->alphaV, rEncodedOne);
		alphaCheck = alphaCheck && encoding->mulEquals(*pk->alphaL, proof->W, lEncodedOne, proof->alphaW);
		alphaCheck = alphaCheck && encoding->mulEquals(proof->H, *(pk->alphaR), proof->alphaH, rEncodedOne);
	}

#ifdef VERBOSE
	if (alphaCheck) {
		printf("\tPassed alpha check!\n");
	} else {
		printf("\tFailed alpha check!\n");
	}
#endif // VERBOSE

	// Check that the beta-gamma term matches	
	bool betaCheck = true;
	if (sk->designatedVerifier) {
		FieldElt betaGammaV, betaGammaW;

		field->mul(sk->betaV, sk->gamma, betaGammaV);
		field->mul(sk->betaW, sk->gamma, betaGammaW);
		
		// We check in two parts.  All of the V terms left on BaseCurve, so we can check directly
		// Sadly, proof->W lives on the TwistCurve, while betaW*W lives on the BaseCurve,
		// so we need two pairings

		// Compute gamma*(betaV*V + betaW*W)
		LEncodedElt gammaZ;
		encoding->mul(proof->beta, sk->gamma, gammaZ);

		// Compute (beta_v*gamma)*V_mid, and same for W
		LEncodedElt vBetaGammaV;
		REncodedElt wBetaGammaW;
	
		encoding->mul(proof->V, betaGammaV, vBetaGammaV);
		encoding->mul(proof->W, betaGammaW, wBetaGammaW);

		// Subtract to get (allegedly) gamma*(betaW*W) 
		LEncodedElt lgammaW;
		encoding->sub(gammaZ, vBetaGammaV, lgammaW);

		// Check that gamma*(betaW*W) == (beta_v*gamma)*W via two pairings
		LEncodedElt lEncodedOne;
		encoding->encode(one, lEncodedOne);	// Enc(1)
		REncodedElt rEncodedOne;
		encoding->encode(one, rEncodedOne);	// Enc(1)

		betaCheck = encoding->mulEquals(lgammaW, rEncodedOne, lEncodedOne, wBetaGammaW);
	} else {
		// Compute gamma*(betaV*V + betaW*W)
		EncodedProduct gammaZ;
		encoding->mul(proof->beta, *pk->gammaR, gammaZ);

		// Compute (beta_v*gamma)*V_mid, and same for W
		EncodedProduct vBetaGammaV, wBetaGammaW;
		encoding->mul(proof->V, *pk->betaVgamma, vBetaGammaV);
		encoding->mul(*pk->betaWgamma, proof->W, wBetaGammaW);

		// Combine to get (gamma*betaV)*V + (gamma*betaW)*W 
		EncodedProduct sum;
		encoding->add(vBetaGammaV, wBetaGammaW, sum);

		// Check that gamma*(betaV*V + betaW*W) = (gamma*betaV)*V + (gamma*betaW)*W
		betaCheck = encoding->equals(gammaZ, sum);
	}

#ifdef VERBOSE
	if (betaCheck) {
		printf("\tPassed beta check!\n");
	} else {
		printf("\tFailed beta check!\n");
	}
#endif // VERBOSE

	checkTimer->stop();

	return quadraticCheck && alphaCheck && betaCheck;
}

#else // !USE_OLD_PROTO

int QSP::genFreshRoots(int num, EltVector& roots) {
	return 0;
}

// Returns a pointer to V_0
SparsePolynomial* QSP::getSPtarget() {
	return NULL;
}

SparsePolynomial* QSP::newSparsePoly() {
	return NULL;
}


#endif