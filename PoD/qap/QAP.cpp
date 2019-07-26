#define WIN32_LEAN_AND_MEAN  // Prevent conflicts in Window.h and WinSock.h
#include <assert.h>
#include "QAP.h"
#include <iostream>

using namespace std;

QAP::QAP(Circuit* circuit, Encoding* encoding, bool cachedBuild) {
	this->encoding = encoding;
	this->field = encoding->getSrcField();
	this->circuit = circuit;	

	assert(circuit->mulGates.size() > 1);	// Lagrange assumes more than 1 root, so we need more than one mult gate

	// See Theorem 12
	this->degree = (int) circuit->mulGates.size();
	this->size = (int) circuit->inputs.size(); // Initial setting

	// Size += mulGates, but some mulGates don't use polynomials, so we need to ask each to figure it out
	for (MulGateVector::iterator iter = circuit->mulGates.begin();
				iter != circuit->mulGates.end();
				iter++) {
		this->size += (*iter)->polyCount();
	}

	this->V = new SparsePolynomial[this->size+1];	// +1 for v_0
	this->W = new SparsePolynomial[this->size+1]; // +1 for w_0
	this->Y = new SparsePolynomial[this->size+1]; // +1 for y_0

	printf("V + W + Y: %d Bytes\n", sizeof(SparsePolynomial) * (this->size+1) * 3);

	// Choose degree roots for the target polynomial
	this->numTargetRoots = 0;

#ifdef USE_OLD_PROTO
	long long numRootsPreallocate = this->degree + 2*this->size;  // Assume we're going to strengthen the QAP
#else
	long long numRootsPreallocate = this->degree;
#endif
	targetRoots = new FieldElt[numRootsPreallocate];	
	genFreshRoots(this->degree);

	printf("TargetRoots ~= %d bytes\n", sizeof(FieldElt)*numRootsPreallocate);
	printf("Building a QAP with size %d and degree %d\n", this->size, this->degree);	
	fflush(stdout);

	// Process each multiplication subcircuit
	//int mulGateCount = 0;
	for (MulGateVector::iterator iter = circuit->mulGates.begin();
		iter != circuit->mulGates.end();
		iter++) {
		(*iter)->generatePolys(this, cachedBuild);
		(*iter)->assignPolyCoeff(this);
		
		//mulGateCount++;
		//if (mulGateCount % 100 == 0 || mulGateCount > 110600) {
		//	printf("Generated polys for mul gate %d out of %d.\n", mulGateCount, circuit->mulGates.size());
		//	printf("# Cached polys: %d.  MaxWireMods: %d.  MaxPolyNzr: %d. MaxCachedPolyNzr: %d. Total NZRs: %d\n",
		//		     GateMul2::cachedCount, GateMul2::maxWireMods, GateMul2::maxPolyNzrCount, GateMul2::maxCachedPolyNzrCount, SparsePolynomial::totalNzrs);
		//	fflush(stdout);
		//}
	}

	printf("Built the QAP. Cleaning up construction debris.\n");
	fflush(stdout);

	// Finally, strengthen the QAP to prevent double wire assignments
#ifdef USE_OLD_PROTO
#ifdef DEBUG_STRENGTHEN
	printf("Warning!  Not strengthening!\n");
#else
	this->strengthen();
	printf("After strengthening, we have a QAP with size %d and degree %d.\n", this->size, this->degree);
	fflush(stdout);
#endif
#endif // USE_OLD_PROTO -- New protocol doesn't need to strengthen!	

	if (cachedBuild) {
		int gateCount = 0;
		// Cleanup the cached polynomials we built during QAP construction
		for (GateList::iterator iter = circuit->gates.begin();
				 iter!= circuit->gates.end();
				 iter++) {
			(*iter)->deleteCachedPolynomial();
			gateCount++;
		}
	}

//#ifdef _DEBUG
	int bothNonZero = 0;
	int nonzero_v = 0;
	int nonzero_w = 0;
	int nonzero_y = 0;
	for (int i = 0; i < this->size+1; i++) {
		if (this->V[i].size() > 0 && this->W[i].size() > 0) {
			bothNonZero++;
		}
		if (this->V[i].size() > 0) { nonzero_v++; }
		if (this->W[i].size() > 0) { nonzero_w++; }
		if (this->Y[i].size() > 0) { nonzero_y++; }
	}
	printf("%d of %d polynomials (%0.2f%%) are non-zero at both V and W\n", bothNonZero, this->size+1, 100*bothNonZero/(float)(this->size+1));
	printf("Total non-zero polynomials: V=%d, W=%d, Y=%d\n", nonzero_v, nonzero_w, nonzero_y);

	nonzero_v = 0;
	nonzero_w = 0;
	nonzero_y = 0;

	// Check input specifically
	for (int i = 0; i < circuit->inputs.size(); i++) {
		uint64_t index = polyIndex(circuit->inputs[i]);
		if (this->V[index].size() > 0) { nonzero_v++; }
		if (this->W[index].size() > 0) { nonzero_w++; }
		if (this->Y[index].size() > 0) { nonzero_y++; }
	}

	// Check output specifically
	for (int i = 0; i < circuit->outputs.size(); i++) {
		int index = polyIndex((GateMul2*)circuit->outputs[i]->input);
		if (this->V[index].size() > 0) { nonzero_v++; }
		if (this->W[index].size() > 0) { nonzero_w++; }
		if (this->Y[index].size() > 0) { nonzero_y++; }
	}

	printf("IO non-zero polynomials: V=%d, W=%d, Y=%d\n", nonzero_v, nonzero_w, nonzero_y);

//#endif
}

// Implements DJBX33X: Treats key as a character string.
// At each stage, multiply hash by 33 and xor in a character
uint32_t insecure_hash(const unsigned char* key, const uint32_t key_len) {
	uint32_t hash = 5381;
	for (unsigned int i = 0; i < key_len; i++) {
		hash = ((hash << 5) + hash) ^ key[i];
	}
	return hash;
}

QAP::~QAP() {
	delete [] targetRoots;

	delete[] this->V;
	delete[] this->W;
	delete[] this->Y;
}

int QAP::genFreshRoots(int num) {
	for (int i = 0; i < num; i++) {
		field->assignFreshElt(&targetRoots[numTargetRoots + i]);
	}

	int ret = numTargetRoots;
	numTargetRoots += num;
	return ret;
}


/* 
 * We use poly[0] for v_0,
 * poly[1...numInputs] for the input wires,
 * and poly[numInputs+1...numInputs+1+numMultGates] for mult gates
 */

long long QAP::polyIndex(Wire* wire) {
	assert(wire->input == NULL && wire->trueInput);		// Must be an input to the circuit
	return 1 + wire->id;
}

long long QAP::polyIndex(GateMul2* gate) {
	return 1 + circuit->inputs.size() + gate->polyId;
}

// Find the wire value corresponding to a particular polynomial
void QAP::wireValueForPolyIndex(long long index, FieldElt& result) {
	if (index == 0 ) {	// No multiplier for p_0(x)
		field->one(result);	
	} else if (index < (long)circuit->inputs.size() + 1) {	// Corresponds to an input
		field->copy(circuit->inputs[index-1]->value, result);
	} else {	// Corresponds to a mult gate
		field->copy(*V[index].coeff, result);		// due to strong QAP, coeff for V, W, and Y are the same
	}
}


void QAP::printPolynomials(SparsePolynomial* polys, int numPolys) {
	//for (int i = 0; i < numPolys; i++) {
	//	if (polys[i].nonZeroRoots.size() > 0) {
	//		printf("%d\t", i);
	//		for (NonZeroRoots::iterator iter = polys[i].nonZeroRoots.begin();
	//		     iter != polys[i].nonZeroRoots.end();
	//		     iter++) {
	//			printf("(%lld, ", (*iter)->rootID);
	//			field->print((*iter)->value);
	//			printf("), ");				
	//		}
	//		printf("\n");
	//	} else {
	//		printf("%d\t All zero\n", i);			
	//	}
	//}
}

void QAP::print() {
	printf("Size: %d, degree: %d\n", size, degree);
	printf("Roots: {");

	for (int i = 0; i < degree; i++) {
		field->print(targetRoots[i]);
		if (i < degree - 1) {
			printf(", ");
		}
	}
	printf("}\n");

	printf("V index : V non-zero roots\n");
	printPolynomials(V, size+1);

	printf("W index : W non-zero roots\n");
	printPolynomials(W, size+1);

	printf("Y index : Y non-zero roots\n");
	printPolynomials(Y, size+1);
}

// Convert this QAP into a strong QAP
void QAP::strengthen() {
	int freshIndex = genFreshRoots(2*this->size);
	FieldElt one;
	field->one(one);

	for (int i = 0; i < this->size; i++) {
		V[0].addNonZeroRoot(freshIndex + this->size + i, one, field);	// v_0(s_i) = 1
		W[0].addNonZeroRoot(freshIndex + i, one, field);  // w_0(r_i) = 1
			
		V[i+1].addNonZeroRoot(freshIndex + i, one, field);	// v_k(r_k) = 1
		W[i+1].addNonZeroRoot(freshIndex + this->size + i, one, field);	// w_k(s_k) = 1
	
		Y[i+1].addNonZeroRoot(freshIndex + i, one, field);	// y_k(r_k) = 1
		Y[i+1].addNonZeroRoot(freshIndex + this->size + i, one, field);	// y_k(s_k) = 1
	}

	this->degree += 2*this->size;
}


FieldElt* QAP::createLagrangeDenominators() {
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
FieldElt* QAP::evalLagrange(FieldElt& evaluationPoint, FieldElt* denominators) {
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
void QAP::evalSparsePoly(SparsePolynomial* poly, FieldElt* lagrange, FieldElt& result) {
	field->zero(result);

	FieldElt tmp;
	for (poly->iterStart(); !poly->iterEnd(); poly->iterNext()) {
		field->mul(lagrange[poly->curRootID()], *poly->curRootVal(), tmp);
		field->add(result, tmp, result);	// result += L_i * c_i
	}
}

// Sometimes, we end up with polynomials represented by arrays,
// with each entry representing the value of the corresponding root
void QAP::evalDensePoly(FieldElt* poly, int polySize, FieldElt* lagrange, FieldElt& result) {
	field->zero(result);

	FieldElt tmp;
	for (int i = 0; i < polySize; i++) {
		field->mul(lagrange[i], poly[i], tmp);
		field->add(result, tmp, result);
	}
}

// Computes: result <- T(evaluationPoint)
void QAP::evalTargetPoly(FieldElt& evaluationPoint, FieldElt& result) {
	field->one(result);

	FieldElt tmp;
	for (int i = 0; i < numTargetRoots; i++) {
		field->sub(evaluationPoint, targetRoots[i], tmp);
		field->mul(result, tmp, result);	// result += evalPt * root[i]		
	}
}




/***************** Old Version of KeyGen *************************/
#ifdef USE_OLD_PROTO
Keys* QAP::genKeys(variables_map& config) {
	Keys* keys = new Keys(config.count("pv") == 0, this, this->degree+1, this->size+1, config.count("nizk") != 0);
	QapPublicKey* pk = (QapPublicKey*)keys->pk;
	QapSecretKey* sk = (QapSecretKey*)keys->sk;

	Timer* randTimer = timers.newTimer("GenRandom", "KeyGen");
	randTimer->start();

	// Select a secret evaluation point (s)
	FieldElt s;
	field->assignRandomElt(&s);
	
	// Select secret multiples to ensure extractability
	FieldElt alpha, betaV, betaW, betaY, gamma;
	field->assignRandomElt(&alpha);
	field->assignRandomElt(&betaV);
	field->assignRandomElt(&betaW);
	field->assignRandomElt(&betaY);
	field->assignRandomElt(&gamma);
	randTimer->stop();

	// Precalculate powers of g to speed up all of the exponentiations below
	int numEncodings =  2*degree   // s^i and alpha*s^i
										+ 1*size*3;  // (W, alphaW, betaW) 
	TIME(encoding->precomputePowersFast(numEncodings, config["mem"].as<int>(), config.count("fill") != 0), "PreKeyPwrs", "KeyGen");

	if (config.count("dv")) {
		Timer* dTimer = timers.newTimer("DvCopyKEA", "KeyGen");	// Copy the knowledge of exponent vals, \alpha, \beta, etc.
		dTimer->start();
		field->copy(alpha, sk->alpha);
		field->copy(betaV, sk->betaV);
		field->copy(betaW, sk->betaW);
		field->copy(betaY, sk->betaY);
		field->copy(gamma, sk->gamma);
		dTimer->stop();
	} 
	
	if (config.count("pv")) {
		Timer* eTimer = timers.newTimer("PvEncodeKEA", "KeyGen");	// Encode the knowledge of exponent vals, \alpha, \beta, etc.
		eTimer->start();

		FieldElt betaVgamma, betaWgamma, betaYgamma;
		field->mul(betaV, gamma, betaVgamma);
		field->mul(betaW, gamma, betaWgamma);
		field->mul(betaY, gamma, betaYgamma);
		encoding->encode(alpha, *pk->alphaL);
		encoding->encode(alpha, *pk->alphaR);
		encoding->encode(gamma, *pk->gammaR);
		encoding->encode(betaVgamma, *pk->betaVgamma);
		encoding->encode(betaWgamma, *pk->betaWgamma);
		encoding->encode(betaYgamma, *pk->betaYgamma);

		eTimer->stop();
	}	
	
	// Create encodings of s^i and alpha*s^i for i \in [0, degree]
	Timer* sTimer = timers.newTimer("PreKeyPwrsOfS", "KeyGen");
	FieldElt sPower;
	field->one(sPower);	// = s^0
	FieldElt alphaSpower;
	field->copy(alpha, alphaSpower); // = alpha*s^0
		
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
	FieldElt* yAtS = new FieldElt[this->size+1];

	// To aid the evaluation of the QAP's polynomials,
	// we precalculate each root's corresponding Lagrange polynomial at s
	// This goes into the public key too, since it aids in the calculation of h(x)
	pk->denominators = createLagrangeDenominators();
	FieldElt* lagrange = evalLagrange(s, pk->denominators);		
	for (int k = 0; k < this->size+1; k++) {
		evalSparsePoly(&V[k], lagrange, vAtS[k]);
		evalSparsePoly(&W[k], lagrange, wAtS[k]);
		evalSparsePoly(&Y[k], lagrange, yAtS[k]);
	}
	delete [] lagrange;
	evalTimer->stop();

	if (config.count("dv")) {
		// Secret key holds v_0, v_in, v_out; all evaluated at s 
		Timer* copyTimer = timers.newTimer("DvCopyVio", "KeyGen");
		copyTimer->start();
		sk->sizeOfv = (int) (1 + circuit->inputs.size() + circuit->outputs.size());
		sk->v = new FieldElt[sk->sizeOfv];  		
		int ioCount = 0;
		for (int i = 0; i < 1 + circuit->inputs.size(); i++) {	// v_0 and v_in
			field->copy(vAtS[i], sk->v[ioCount++]);
		}

		for (WireVector::iterator iter = circuit->outputs.begin();	// v_out
			 iter != circuit->outputs.end();
			 iter++) {
			field->copy(vAtS[polyIndex((GateMul2*)(*iter)->input)], sk->v[ioCount++]);
		}
		copyTimer->start();
	} 
	
	if (config.count("pv")) {
		Timer* ioTimer = timers.newTimer("PkEncodeIO", "KeyGen");
		ioTimer->start();

		// Public key needs to hold v_in and v_out for verification
		// Could be calculated by verifier from key, but that makes verification more expensive
		for (int i = 1; i < circuit->inputs.size() + 1; i++) {	//  v_in
			encoding->encode(vAtS[i], pk->V[i]);
		}

		for (WireVector::iterator iter = circuit->outputs.begin();	// v_out
			 iter != circuit->outputs.end();
			 iter++) {
			encoding->encode(vAtS[polyIndex((GateMul2*)(*iter)->input)], pk->V[polyIndex((GateMul2*)(*iter)->input)]);
		}
		ioTimer->stop();
	}

	// Encode the evaluated polynomials
	Timer* encodeTimer = timers.newTimer("EncodePolys", "KeyGen");
	Timer* baseEncodeTimer = timers.newTimer("BaseEncoding", "EncodePolys");
	Timer* twistEncodeTimer = timers.newTimer("TwistEncoding", "EncodePolys");
	encodeTimer->start();

	// If PUSH_EXTRA_WORK_TO_WORKER, let the worker do: V, Y, \alphaV, \alphaY using the s^i and \alpha*s^i
	// Can't use those for W though, since it's on the TwistCurve.
	// Could do s^i and \alpha*s^i on the Twist too, but at present, degree ~= 3*size,
	// so that would add two sets of degree exponentiations, in exchange for W and \alpha W, which are two sets of size exp.
	for (int k = 0; k < this->size+1; k++) {
#ifdef PUSH_EXTRA_WORK_TO_WORKER
		if (k == 0) {		// Still need v_0 and w_0 in the key
#endif
			baseEncodeTimer->start();
			encoding->encode(vAtS[k], pk->V[k]);		
			encoding->encode(yAtS[k], pk->Y[k]);
			baseEncodeTimer->stop();
#ifdef PUSH_EXTRA_WORK_TO_WORKER
		}
#endif 

		twistEncodeTimer->start();
		encoding->encode(wAtS[k], pk->W[k]);	
		twistEncodeTimer->stop();
	}

	// Calculate \alpha times the evaluated polynomials
	for (int k = 0; k < this->size+1; k++) {
#ifdef PUSH_EXTRA_WORK_TO_WORKER
		if (k == 0) {		// Still need v_0 and w_0 in the key
#endif
			field->mul(alpha, vAtS[k], vAtS[k]);
			field->mul(alpha, yAtS[k], yAtS[k]);
#ifdef PUSH_EXTRA_WORK_TO_WORKER
		}
#endif
		field->mul(alpha, wAtS[k], wAtS[k]);
	}

	// Encode \alpha times the evaluated polynomials
	for (int k = 0; k < this->size+1; k++) {
#ifdef PUSH_EXTRA_WORK_TO_WORKER
		if (k == 0) {		// Still need v_0 and w_0 in the key
#endif
			baseEncodeTimer->start();
			encoding->encode(vAtS[k], pk->alphaV[k]);		
			encoding->encode(yAtS[k], pk->alphaY[k]);
			baseEncodeTimer->stop();
#ifdef PUSH_EXTRA_WORK_TO_WORKER
		}
#endif

		twistEncodeTimer->start();
		encoding->encode(wAtS[k], pk->alphaW[k]);
		twistEncodeTimer->stop();
	}

	// Calculate \beta times the evaluated polynomials
	FieldElt betaVOverAlpha, betaWOverAlpha, betaYOverAlpha;
	field->div(betaV, alpha, betaVOverAlpha);
	field->div(betaW, alpha, betaWOverAlpha);
	field->div(betaY, alpha, betaYOverAlpha);

	for (int k = 0; k < this->size+1; k++) {
#ifdef PUSH_EXTRA_WORK_TO_WORKER
		if (k == 0) {
			field->mul(betaVOverAlpha, vAtS[k], vAtS[k]);
			field->mul(betaYOverAlpha, yAtS[k], yAtS[k]);
		} else {
			field->mul(betaV, vAtS[k], vAtS[k]);
			field->mul(betaY, yAtS[k], yAtS[k]);
		}
#else
		field->mul(betaVOverAlpha, vAtS[k], vAtS[k]);
		field->mul(betaYOverAlpha, yAtS[k], yAtS[k]);
#endif
		
		field->mul(betaWOverAlpha, wAtS[k], wAtS[k]);		
	}

	// Encode \beta times the evaluated polynomials.  Still have to do this for all the polys
	for (int k = 0; k < this->size+1; k++) {
		baseEncodeTimer->start();
		encoding->encode(vAtS[k], pk->betaV[k]);	
		encoding->encode(wAtS[k], pk->betaW[k]);	
		encoding->encode(yAtS[k], pk->betaY[k]);
		baseEncodeTimer->stop();
	}
	encodeTimer->stop();

	// Precompute the polynomial tree needed to quickly compute h(x)
	// Let the worker handle this
	//TIME(pk->polyTree = field->polyTree(targetRoots, numTargetRoots), "PrePolyTree", "OneTimeWork");
	TIME(evalTargetPoly(s, sk->target), "ComputeTatS", "KeyGen");
	
	if (config.count("pv")) {
		TIME(encoding->encode(sk->target, *pk->RtAtS), "EncodeTatS", "KeyGen");

		if (config.count("nizk")) {
			Timer* nizkTimer = timers.newTimer("EncodeNIZK", "KeyGen");	// Encode the extra values we need for NIZKs
			nizkTimer->start();

			pk->LtAtS = (LEncodedElt*) new LEncodedElt;
			pk->LalphaTatS = (LEncodedElt*) new LEncodedElt;
			pk->RalphaTatS = (REncodedElt*) new REncodedElt;
			pk->LbetaTatS = (LEncodedElt*) new LEncodedElt;
			pk->betaYTatS = (LEncodedElt*) new LEncodedElt;

			encoding->encode(sk->target, *pk->LtAtS);

			FieldElt tmp;
			field->mul(alpha, sk->target, tmp);
			encoding->encode(tmp, *pk->LalphaTatS);
			encoding->encode(tmp, *pk->RalphaTatS);
			field->mul(betaV, sk->target, tmp);
			encoding->encode(tmp, *pk->LbetaTatS);
			field->mul(betaY, sk->target, tmp);
			encoding->encode(tmp, *pk->betaYTatS);

			nizkTimer->stop();
		}
	}

	// In practice, we might save these for a future computation,
	// but for now, we delete them to ease memory pressure
	encoding->deletePrecomputePowers();

	delete [] vAtS;
	delete [] wAtS;
	delete [] yAtS;

	return keys;
}

#else // !USE_OLD_PROTO
/***************** New Version of KeyGen *************************/

Keys* QAP::genKeys(variables_map& config) {
	Keys* keys = new Keys(config.count("pv") == 0, this, this->degree+1, this->size+1, config.count("nizk") != 0);
	QapPublicKey* pk = (QapPublicKey*)keys->pk;
	QapSecretKey* sk = (QapSecretKey*)keys->sk;

	Timer* randTimer = timers.newTimer("GenRandom", "KeyGen");
	randTimer->start();

	// Select a secret evaluation point (s)
	FieldElt s;
	field->assignRandomElt(&s);
	
	// Select secret multiples to ensure extractability
	FieldElt alphaV, alphaW, alphaY;
	FieldElt beta;	
	FieldElt gamma;
	FieldElt rV, rW, rY;

	field->assignRandomElt(&alphaV);
	field->assignRandomElt(&alphaW);
	field->assignRandomElt(&alphaY);
	field->assignRandomElt(&beta);
	field->assignRandomElt(&gamma);
	field->assignRandomElt(&rV);
	field->assignRandomElt(&rW);
	field->mul(rV, rW, rY);
	
	randTimer->stop();

	// Precalculate powers of g to speed up all of the exponentiations below
	int numEncodings =  2*degree   // s^i and alpha*s^i
										+ (3*2+1)*size;  // (v_k, w_k, y_k), (alpha v_k, alpha w_k, alpha y_k), beta
	TIME(encoding->precomputePowersFast(numEncodings, config["mem"].as<int>(), config.count("fill") != 0), "PreKeyPwrs", "KeyGen");

	if (config.count("dv")) {
		Timer* dTimer = timers.newTimer("DvCopyKEA", "KeyGen");	// Copy the knowledge of exponent vals, \alpha, \beta, etc.
		dTimer->start();
		field->copy(alphaV, sk->alphaV);
		field->copy(alphaW, sk->alphaW);
		field->copy(alphaY, sk->alphaY);
		field->copy(beta, sk->beta);
		field->copy(gamma, sk->gamma);
		field->copy(rV, sk->rV);
		field->copy(rW, sk->rW);
		field->copy(rY, sk->rY);
		encoding->encode(alphaW, *pk->alphaW);		// We need this for a pairing later, since Wpoly is on twist, but alphaWpoly is on base
		dTimer->stop();
	} 
	
	if (config.count("pv")) {
		Timer* eTimer = timers.newTimer("PvEncodeKEA", "KeyGen");	// Encode the knowledge of exponent vals, \alpha, \beta, etc.
		eTimer->start();

		encoding->encode(alphaV, *pk->alphaV);
		encoding->encode(alphaW, *pk->alphaW);
		encoding->encode(alphaY, *pk->alphaY);

		encoding->encode(gamma, *pk->gammaR);

		FieldElt betaGamma;
		field->mul(beta, gamma, betaGamma);
		encoding->encode(betaGamma, *pk->betaGammaL);
		encoding->encode(betaGamma, *pk->betaGammaR);

		eTimer->stop();
	}	
	
	// Create encodings of s^i for i \in [0, degree]
	Timer* sTimer = timers.newTimer("PreKeyPwrsOfS", "KeyGen");
	FieldElt sPower;
	field->one(sPower);	// = s^0
		
	sTimer->start();
	for (int i = 0; i < this->degree + 1; i++) {		
		encoding->encode(sPower, pk->powers[i]);
		field->mul(sPower, s, sPower);
	}
	sTimer->stop();	

	// Evaluate each polynomial at s
	Timer* evalTimer = timers.newTimer("EvalAtS", "KeyGen");
	evalTimer->start();

	FieldElt* vAtS = new FieldElt[this->size+1];
	FieldElt* wAtS = new FieldElt[this->size+1];
	FieldElt* yAtS = new FieldElt[this->size+1];

	// To aid the evaluation of the QAP's polynomials,
	// we precalculate each root's corresponding Lagrange polynomial at s
	// This goes into the public key too, since it aids in the calculation of h(x)
	pk->denominators = createLagrangeDenominators();
	FieldElt* lagrange = evalLagrange(s, pk->denominators);		
	for (int k = 0; k < this->size+1; k++) {
		evalSparsePoly(&V[k], lagrange, vAtS[k]);
		evalSparsePoly(&W[k], lagrange, wAtS[k]);
		evalSparsePoly(&Y[k], lagrange, yAtS[k]);
	}
	delete [] lagrange;
	evalTimer->stop();

	if (config.count("dv")) {
			Timer* copyTimer = timers.newTimer("DvCopyVio", "KeyGen");
		copyTimer->start();
#ifndef CHECK_NON_ZERO_IO
		// Secret key holds r1*v_0, r1*v_in, r1*v_out; all evaluated at s 
		sk->sizeOfv = (int) (1 + circuit->inputs.size() + circuit->outputs.size());
		sk->v = new FieldElt[sk->sizeOfv];  		
		int ioCount = 0;
		for (int i = 0; i < 1 + circuit->inputs.size(); i++) {	// v_0 and v_in
			field->mul(rV, vAtS[i], sk->v[ioCount++]);
		}

		for (WireVector::iterator iter = circuit->outputs.begin();	// v_out
			 iter != circuit->outputs.end();
			 iter++) {
			field->mul(rV, vAtS[polyIndex((GateMul2*)(*iter)->input)], sk->v[ioCount++]);
		}		
#else // Defined CHECK_NON_ZERO_IO
		// Secret key holds r1*v_in, r2*alpha*w_in, r3*y_out; all evaluated at s 
		sk->sizeOfv = (int) circuit->inputs.size() - (int) circuit->witness.size();
		sk->numWs = (int) circuit->inputs.size() - (int) circuit->witness.size();
		sk->numYs = (int) (circuit->inputs.size() + circuit->outputs.size() - (int) circuit->witness.size());	// On input-level split gates, y is non-zero for some inputs

		sk->v = new FieldElt[sk->sizeOfv];  
		sk->alphaWs = new FieldElt[sk->numWs];  
		sk->y = new FieldElt[sk->numYs];  

		FieldElt rWalphaW;
		field->mul(rW, alphaW, rWalphaW);
		for (int i = 0; i < circuit->inputs.size() - circuit->witness.size(); i++) {
			field->mul(rV, vAtS[i+1], sk->v[i]);		// v_in. vAtS uses +1 to skip v_0
			field->mul(rWalphaW, wAtS[i+1], sk->alphaWs[i]); // w_in
			field->mul(rY, yAtS[i+1], sk->y[i]);
		}

		size_t i = circuit->inputs.size();
		for (WireVector::iterator iter = circuit->outputs.begin();	// y_out
			 iter != circuit->outputs.end();
			 iter++) {
			field->mul(rY, yAtS[polyIndex((GateMul2*)(*iter)->input)], sk->y[i++]);
		}	
#endif
		copyTimer->stop();
	} 

	// 4/23/13 -- This appears to be redundant now
	//if (config.count("pv")) {
	//	Timer* ioTimer = timers.newTimer("PkEncodeIO", "KeyGen");
	//	ioTimer->start();

	//	// Public key needs to hold v_in and v_out for verification
	//	// Could be calculated by verifier from key, but that makes verification more expensive
	//	for (int i = 1; i < circuit->inputs.size() + 1; i++) {	//  v_in
	//		FieldElt rVvAtS;
	//		field->mul(rV, vAtS[i], rVvAtS);
	//		encoding->encode(rVvAtS, pk->Vpolys[i]);
	//	}

	//	for (WireVector::iterator iter = circuit->outputs.begin();	// v_out
	//		 iter != circuit->outputs.end();
	//		 iter++) {
	//		FieldElt rVvAtS;
	//		field->mul(rV, vAtS[polyIndex((GateMul2*)(*iter)->input)], rVvAtS);
	//		encoding->encode(rVvAtS, pk->Vpolys[polyIndex((GateMul2*)(*iter)->input)]);
	//	}
	//	ioTimer->stop();
	//}

	// Encode the evaluated polynomials
	Timer* encodeTimer = timers.newTimer("EncodePolys", "KeyGen");
	Timer* baseEncodeTimer = timers.newTimer("BaseEncoding", "EncodePolys");
	Timer* twistEncodeTimer = timers.newTimer("TwistEncoding", "EncodePolys");
	encodeTimer->start();
	
	for (int k = 0; k < this->size+1; k++) {
		baseEncodeTimer->start();

		FieldElt rVatS, rYatS;

		field->mul(rV, vAtS[k], rVatS);
		encoding->encode(rVatS, pk->Vpolys[k]);

		field->mul(rY, yAtS[k], rYatS);
		encoding->encode(rYatS, pk->Ypolys[k]);

		baseEncodeTimer->stop();

		twistEncodeTimer->start();

		FieldElt rWatS;
		field->mul(rW, wAtS[k], rWatS);
		encoding->encode(rWatS, pk->Wpolys[k]);	
		twistEncodeTimer->stop();
	}

	// Calculate \alpha times the evaluated polynomials (overwriting the old values to save space)
	FieldElt rValphaV, rWalphaW, rYalphaY;
	field->mul(rV, alphaV, rValphaV);
	field->mul(rW, alphaW, rWalphaW);
	field->mul(rY, alphaY, rYalphaY);
	for (int k = 0; k < this->size+1; k++) {
		field->mul(rValphaV, vAtS[k], vAtS[k]);
		field->mul(rWalphaW, wAtS[k], wAtS[k]);
		field->mul(rYalphaY, yAtS[k], yAtS[k]);
	}

	// Encode \alpha times the evaluated polynomials
	for (int k = 0; k < this->size+1; k++) {
		baseEncodeTimer->start();
		encoding->encode(vAtS[k], pk->alphaVpolys[k]);		
		encoding->encode(wAtS[k], pk->alphaWpolys[k]);
		encoding->encode(yAtS[k], pk->alphaYpolys[k]);
		baseEncodeTimer->stop();
	}

	// Calculate \beta times the evaluated polynomials.
	// This requires multiplying by beta and dividing by the appropriate alpha
	// that we added above
	FieldElt betaOverAlphaV, betaOverAlphaW, betaOverAlphaY;
	field->div(beta, alphaV, betaOverAlphaV);
	field->div(beta, alphaW, betaOverAlphaW);
	field->div(beta, alphaY, betaOverAlphaY);

	// Combine all of the beta poly evaluations into a single term:
	// rV*\betaV*v_k(s) + rW*\betaW*w_k(s) + rY*\betaY*y_k(s)
	// (Note that the r terms are already present from when we added alpha)
	for (int k = 0; k < this->size+1; k++) {
		// Compute individual beta terms
		field->mul(betaOverAlphaV, vAtS[k], vAtS[k]);
		field->mul(betaOverAlphaW, wAtS[k], wAtS[k]);
		field->mul(betaOverAlphaY, yAtS[k], yAtS[k]);		
		// Combine.  Store in v to save space
		field->add(vAtS[k], wAtS[k], vAtS[k]);
		field->add(vAtS[k], yAtS[k], vAtS[k]);
	}

	// Encode \beta times the evaluated polynomials.  
	for (int k = 0; k < this->size+1; k++) {
		baseEncodeTimer->start();
		encoding->encode(vAtS[k], pk->betaVWYpolys[k]);	
		baseEncodeTimer->stop();
	}
	encodeTimer->stop();

	// Precompute the polynomial tree needed to quickly compute h(x)
	// Let the worker handle this
	//TIME(pk->polyTree = field->polyTree(targetRoots, numTargetRoots), "PrePolyTree", "OneTimeWork");

	TIME(evalTargetPoly(s, sk->target), "ComputeTatS", "KeyGen");
	
	if (config.count("pv")) {
		// Encode the target on g_y
		FieldElt rYtarget;
		field->mul(rY, sk->target, rYtarget);
		TIME(encoding->encode(rYtarget, *pk->RtAtS), "EncodeTatS", "KeyGen");

		if (config.count("nizk")) {
			Timer* nizkTimer = timers.newTimer("EncodeNIZK", "KeyGen");	// Encode the extra values we need for NIZKs
			nizkTimer->start();

			pk->LVtAtS = (LEncodedElt*) new LEncodedElt;
			pk->RWtAtS = (REncodedElt*) new REncodedElt;
			pk->LYtAtS = (LEncodedElt*) new LEncodedElt;
			pk->LalphaVTatS = (LEncodedElt*) new LEncodedElt;
			pk->LalphaWTatS = (LEncodedElt*) new LEncodedElt;
			pk->LalphaYTatS = (LEncodedElt*) new LEncodedElt;
			pk->LbetaVTatS = (LEncodedElt*) new LEncodedElt;
			pk->LbetaWTatS = (LEncodedElt*) new LEncodedElt;
			pk->LbetaYTatS = (LEncodedElt*) new LEncodedElt;

			FieldElt tmp, betaTatS;

			field->mul(sk->target, rV, tmp);
			encoding->encode(tmp, *pk->LVtAtS);
			field->mul(sk->target, rW, tmp);
			encoding->encode(tmp, *pk->RWtAtS);
			field->mul(sk->target, rY, tmp);
			encoding->encode(tmp, *pk->LYtAtS);

			field->mul(alphaV, sk->target, tmp);
			field->mul(tmp, rV, tmp);	
			encoding->encode(tmp, *pk->LalphaVTatS);  // == g_v^{alphaV*t(s)}
			field->mul(alphaW, sk->target, tmp);
			field->mul(tmp, rW, tmp);	
			encoding->encode(tmp, *pk->LalphaWTatS);  // == g_w^{alphaW*t(s)}
			field->mul(alphaY, sk->target, tmp);
			field->mul(tmp, rY, tmp);	
			encoding->encode(tmp, *pk->LalphaYTatS);  // == g_w^{alphaY*t(s)}

			field->mul(beta, sk->target, betaTatS);
			field->mul(betaTatS, rV, tmp);
			encoding->encode(tmp, *pk->LbetaVTatS);
			field->mul(betaTatS, rW, tmp);
			encoding->encode(tmp, *pk->LbetaWTatS);
			field->mul(betaTatS, rY, tmp);
			encoding->encode(tmp, *pk->LbetaYTatS);

			nizkTimer->stop();
		}
	}

	// In practice, we might save these for a future computation,
	// but for now, we delete them to ease memory pressure
	encoding->deletePrecomputePowers();

	delete [] vAtS;
	delete [] wAtS;
	delete [] yAtS;

	return keys;
}

#endif // USE_OLD_PROTO


// Apply a set of coefficients to a set of encoded values
template <typename EncodedEltType>
void QAP::applyCoefficients(MultiExpPrecompTable<EncodedEltType>& precompTable, long numCoefficients, FieldElt* coefficients, LEncodedElt& result) {
	encoding->multiMul(precompTable, coefficients, (int)numCoefficients, result);
}

#ifndef CHECK_NON_ZERO_IO

// Helper function for applyCircuitCoefficients
// result += key[k] * wire[k]
template <typename EncodedEltType>
void QAP::defineWireValues(EncodedEltType** bases, FieldElt** exps, int offset, WireVector& wires, EncodedEltType* key) {
	for (long i = 0; i < (long)wires.size(); i++) {
		long long pIndex = 0;
		if (wires[i]->trueInput) { // Input wire
			assert(wires[i]->input == NULL);
			pIndex = polyIndex(wires[i]);
		} else {	// Mult gate output
			pIndex = polyIndex((GateMul2*)wires[i]->input);
		}

		// a_i * Encoding[poly_i]
		long offsetI = i + offset;
		bases[offsetI] = &key[pIndex]; 
		exps[offsetI]  = &wires[i]->value; 
	}

}

// Take an evaluated circuit and compute an inner product between
// the encoded values in the key and the coefficients of the circuit's wires
// If midOnly is true, ignore the I/O wire values
template <typename EncodedEltType>
void QAP::applyCircuitCoefficients(variables_map& config, EncodedEltType* key, EncodedEltType& result, bool midOnly, Timer* timer) {
	// Setup a descriptor so we can do all of the multiplication work at once
	size_t len = circuit->intermediates.size();
	if (!midOnly) {
		len += circuit->inputs.size();
		len += circuit->outputs.size();
	}

	if (len == 0) { // No work to do
		encoding->zero(result);
		return; 
	}	

	EncodedEltType** bases = new EncodedEltType*[len];
	FieldElt** exps = new FieldElt*[len];

	size_t offset = 0;
	defineWireValues(bases, exps, (int)offset, circuit->intermediates, key);
	offset += circuit->intermediates.size();

	if (!midOnly) {
		defineWireValues(bases, exps, (int)offset, circuit->inputs, key);
		offset += circuit->inputs.size();
		defineWireValues(bases, exps, (int)offset, circuit->outputs, key);
		offset += circuit->outputs.size();
	}

	// Do the actual mult work.  In practice, we'd separate out the precomp and work, but for now, do them together
	MultiExpPrecompTable<EncodedEltType> precompTable;
	timer->stop(); // Don't count the pre-compute time
	TIME(encoding->precomputeMultiPowers(precompTable, bases, (int)len, 1, config["mem"].as<int>(), config.count("fill") != 0), "PreWorkCompMulti", "OneTimeWork");
	timer->start();	// Do count the actual application
	encoding->multiMul(precompTable, exps, (int)len, result);
	timer->stop(); // Don't count the deletion time
	encoding->deleteMultiTables(precompTable);
	timer->start();	// Do count everything else

	delete [] bases;
	delete [] exps;
}
#endif 

// Helper function for applyCircuitCoefficients
// result += key[k] * wire[k]
template <typename EncodedEltType>
void QAP::defineMidWireValues(EncodedEltType** bases, FieldElt** exps, int offset, WireVector& wires, EncodedEltType* key) {
	for (long i = 0; i < (long)wires.size(); i++) {
		long long pIndex = polyIndex((GateMul2*)wires[i]->input);

		// a_i * Encoding[poly_i]
		long offsetI = i + offset;
		bases[offsetI] = &key[pIndex]; 
		exps[offsetI]  = &wires[i]->value; 
	}

}

// Helper function for applyCircuitCoefficients
// result += key[k] * wire[k]
template <typename EncodedEltType>
int QAP::defineInputWireValues(EncodedEltType** bases, FieldElt** exps, int offset, WireVector& wires, EncodedEltType* key) {
	int nonZeroCount = 0;
	for (long i = 0; i < (long)wires.size(); i++) {
		assert(wires[i]->input == NULL);
		long long pIndex = polyIndex(wires[i]);
	
		// Only include values where W is a non-zero polynomial
		if (W[pIndex].size() > 0) {			// TODO: At the moment, this is the only one we care about.  In theory, we might want to generalize to other polys
			// a_i * Encoding[poly_i]
			long offsetI = nonZeroCount + offset;
			bases[offsetI] = &key[pIndex]; 
			exps[offsetI]  = &wires[i]->value; 
			nonZeroCount++;
		}
	}

	return nonZeroCount;
}


// Helper function for applyCircuitCoefficients
// result += key[k] * wire[k]
template <typename EncodedEltType>
void QAP::defineWitnessWireValues(EncodedEltType** bases, FieldElt** exps, int offset, WireVector& wires, EncodedEltType* key) {
	for (long i = 0; i < (long)wires.size(); i++) {
		assert(wires[i]->input == NULL);
		long long pIndex = polyIndex(wires[i]);
	
		// a_i * Encoding[poly_i]
		long offsetI = i + offset;
		bases[offsetI] = &key[pIndex]; 
		exps[offsetI]  = &wires[i]->value; 
	}
}

// Take an evaluated circuit and compute an inner product between
// the encoded values in the key and the coefficients of the circuit's wires
// If midOnly is true, ignore the I/O wire values
template <typename EncodedEltType>
void QAP::applyCircuitCoefficients(variables_map& config, EncodedEltType* key, EncodedEltType& result, bool inclNonZeroWinputs, Timer* timer) {
	// Setup a descriptor so we can do all of the multiplication work at once
	size_t max_len = circuit->intermediates.size() + circuit->witness.size();
	if (inclNonZeroWinputs) {
		max_len += circuit->inputs.size();
	}

	if (max_len == 0) { // No work to do
		encoding->zero(result);
		return; 
	}	

	EncodedEltType** bases = new EncodedEltType*[max_len];
	FieldElt** exps = new FieldElt*[max_len];

	size_t offset = 0;
	defineMidWireValues(bases, exps, (int)offset, circuit->intermediates, key);
	offset += circuit->intermediates.size();

	if (inclNonZeroWinputs) {
		int numNonZero = defineInputWireValues(bases, exps, (int)offset, circuit->inputs, key);
		offset += numNonZero;
	} else {
		defineWitnessWireValues(bases, exps, (int)offset, circuit->witness, key);
		offset += circuit->witness.size();
	}

	// Do the actual mult work.  In practice, we'd separate out the precomp and work, but for now, do them together
	MultiExpPrecompTable<EncodedEltType> precompTable;
	timer->stop(); // Don't count the pre-compute time
	TIME(encoding->precomputeMultiPowers(precompTable, bases, (int)offset, 1, config["mem"].as<int>(), config.count("fill") != 0), "PreWorkCompMulti", "OneTimeWork");
	timer->start();	// Do count the actual application
	encoding->multiMul(precompTable, exps, (int)offset, result);
	timer->stop(); // Don't count the deletion time
	encoding->deleteMultiTables(precompTable);
	timer->start();	// Do count everything else

	delete [] bases;
	delete [] exps;
}


// Helper function for applyCircuitCoefficients
// result += key[k] * wire[k]
template <typename EncodedEltType>
void QAP::defineAllWireValues(EncodedEltType** bases, FieldElt** exps, int offset, WireVector& wires, EncodedEltType* key) {
	for (long i = 0; i < (long)wires.size(); i++) {
		long long pIndex = 0;
		if (wires[i]->trueInput) { // Input wire
			assert(wires[i]->input == NULL);
			pIndex = polyIndex(wires[i]);
		} else {	// Mult gate output
			pIndex = polyIndex((GateMul2*)wires[i]->input);
		}

		// a_i * Encoding[poly_i]
		long offsetI = i + offset;
		bases[offsetI] = &key[pIndex]; 
		exps[offsetI]  = &wires[i]->value; 
	}

}

// Now used only for beta terms 
// Take an evaluated circuit and compute an inner product between
// the encoded values in the key and the coefficients of the circuit's wires
// If midOnly is true, ignore the I/O wire values
template <typename EncodedEltType>
void QAP::applyAllCircuitCoefficients(variables_map& config, EncodedEltType* key, EncodedEltType& result, Timer* timer) {
	// Setup a descriptor so we can do all of the multiplication work at once
	size_t len = circuit->intermediates.size();
	len += circuit->inputs.size();
	len += circuit->outputs.size();

	if (len == 0) { // No work to do
		encoding->zero(result);
		return; 
	}	

	EncodedEltType** bases = new EncodedEltType*[len];
	FieldElt** exps = new FieldElt*[len];

	size_t offset = 0;
	defineAllWireValues(bases, exps, (int)offset, circuit->intermediates, key);
	offset += circuit->intermediates.size();
	defineAllWireValues(bases, exps, (int)offset, circuit->inputs, key);
	offset += circuit->inputs.size();
	defineAllWireValues(bases, exps, (int)offset, circuit->outputs, key);
	offset += circuit->outputs.size();

	// Do the actual mult work.  In practice, we'd separate out the precomp and work, but for now, do them together
	MultiExpPrecompTable<EncodedEltType> precompTable;
	timer->stop(); // Don't count the pre-compute time
	TIME(encoding->precomputeMultiPowers(precompTable, bases, (int)len, 1, config["mem"].as<int>(), config.count("fill") != 0), "PreWorkCompMulti", "OneTimeWork");
	timer->start();	// Do count the actual application
	encoding->multiMul(precompTable, exps, (int)len, result);
	timer->stop(); // Don't count the deletion time
	encoding->deleteMultiTables(precompTable);
	timer->start();	// Do count everything else

	delete [] bases;
	delete [] exps;
}

// Add p's contribution to the condensed poly in result
void QAP::condensePoly(SparsePolynomial* p, FieldElt& a_i, FieldElt* result) {
		FieldElt tmp;
		for (p->iterStart(); !p->iterEnd(); p->iterNext()) {
			field->mul(a_i, *p->curRootVal(), tmp);
			field->add(result[p->curRootID()], tmp, result[p->curRootID()]);
		}
}

// Combine a set of sparse polynomials into a single dense polynomial
// In both cases, the polynomials are in evaluation form (evaluated at targetRoots)
FieldElt* QAP::computeDensePoly(SparsePolynomial* polys) {
	FieldElt* result = new FieldElt[degree];
	field->zero(result, degree);	

	for (int i = 0; i < size+1; i++) {
		// Find the a_i corresponding to this polynomial
		FieldElt a_i;
		
		wireValueForPolyIndex(i, a_i);

		// Compute p_i's contribution to p(x)
		
		condensePoly(&polys[i], a_i, result);
	}

	return result;	
}

// Combine a set of sparse polynomials into a single dense polynomial
// In both cases, the polynomials are in evaluation form (evaluated at targetRoots)
FieldElt* QAP::computeDensePolyIO(SparsePolynomial* polys) {
	FieldElt* result = new FieldElt[degree];
	field->zero(result, degree);	

	// Deal with the inputs and outputs
	for (int i = 0; i < (int) circuit->inputs.size(); i++) {
		SparsePolynomial* p_i = &polys[polyIndex(circuit->inputs[i])];
		condensePoly(p_i, circuit->inputs[i]->value, result);
	}

	for (int i = 0; i < circuit->outputs.size(); i++) {
		SparsePolynomial* p_i = &polys[polyIndex((GateMul2*)circuit->outputs[i]->input)];
		condensePoly(p_i, circuit->outputs[i]->value, result);
	}

	// Finally, add in V_0/W_0/Y_0
	FieldElt one;
	field->one(one);	// No coefficient for the 0 poly
	condensePoly(&polys[0], one, result);

	return result;	
}

FieldElt* QAP::computeDensePolyMid(SparsePolynomial* polys) {
	FieldElt* result = new FieldElt[degree];
	field->zero(result, degree);	

	// Iterate through the mul gate outputs, skipping any that are actually circuit outputs
	for (int i = 0; i < circuit->mulGates.size(); i++) {
		if (circuit->mulGates[i]->output != NULL && circuit->mulGates[i]->output->output == NULL) {
			// This is a real output of the circuit
			continue;
		}

		if (circuit->mulGates[i]->polyCount() > 0) {	// This guy contributes one or more polys
			// Currently we only expect at most 1 poly per mulGate
			assert(circuit->mulGates[i]->polyCount()==1);

			// Find this guy's polynomial and add it's contribution to the final result
			SparsePolynomial* p_i = &polys[polyIndex(circuit->mulGates[i])];
			condensePoly(p_i, *p_i->coeff, result);
		}
	}

	return result;	
}

void QAP::applySparsePoly(SparsePolynomial* poly, LEncodedElt* lagrange, LEncodedElt& result) {
	encoding->zero(result);
	LEncodedElt tmp;
	for (poly->iterStart(); !poly->iterEnd(); poly->iterNext()) {
		encoding->mul(lagrange[poly->curRootID()], *poly->curRootVal(), tmp);
		encoding->add(result, tmp, result);
	}
}

void QAP::applySparsePolysMid(SparsePolynomial* polys, LEncodedElt* lagrange, LEncodedElt* result) {
	// Iterate through the mul gate outputs, skipping any that are actually circuit outputs
	for (int i = 0; i < circuit->mulGates.size(); i++) {
		if (circuit->mulGates[i]->output != NULL && circuit->mulGates[i]->output->output == NULL) {
			// This is a real output of the circuit
			continue;
		}

		if (circuit->mulGates[i]->polyCount() > 0) {	// This guy contributes one or more polys
			// Currently we only expect at most 1 poly per mulGate
			assert(circuit->mulGates[i]->polyCount()==1);
			SparsePolynomial* p_i = &polys[polyIndex(circuit->mulGates[i])];
			applySparsePoly(p_i, lagrange, result[polyIndex(circuit->mulGates[i])]);
		}
	}
}

void QAP::applySparsePolys(SparsePolynomial* polys, LEncodedElt* lagrange, LEncodedElt* result) {
	for (int i = 0; i < size+1; i++) {
		applySparsePoly(&polys[i], lagrange, result[i]);
	}
}

Poly* QAP::interpolate(PublicKey* pk, FieldElt* poly) {
	return Poly::interpolate(field, poly, numTargetRoots, pk->polyTree, pk->denominators);
}

Poly* QAP::getTarget(PublicKey* pk) {
	return pk->polyTree->polys[pk->polyTree->height-1][0];	// Root of the tree is product of all the (x-r_i) terms
}


/***************** Old Version of DoWork *************************/
#ifdef USE_OLD_PROTO
Proof* QAP::doWork(PublicKey* pubKey, variables_map& config) {
	QapPublicKey* pk = (QapPublicKey*)pubKey;
	Proof* proof = new Proof((int)circuit->inputs.size(), (int)circuit->outputs.size());

	// Sometimes the verifier is nice and builds these for us.  Sometimes he isn't so nice.
	if (pk->polyTree == NULL) {
		TIME(pk->polyTree = new PolyTree(field, targetRoots, numTargetRoots), "PreWorkPolyTree", "OneTimeWork");
	}

	// Sometimes the verifier is nice and builds these for us.  Sometimes he isn't so nice.
	if (pk->denominators == NULL) {
		TIME(pk->denominators = createLagrangeDenominators(), "PreWorkDenomin", "OneTimeWork");
	}

	// Copy over the inputs and outputs
	for (int i = 0; i < (int) circuit->inputs.size(); i++) {
		field->copy(circuit->inputs[i]->value, proof->inputs[i]);
	}

	for (int i = 0; i < circuit->outputs.size(); i++) {
		field->copy(circuit->outputs[i]->value, proof->outputs[i]);
	}

	///////////  First, derive h(x): t(x)h(x) = v(x)w(x) - y(x) ///////////////////////
	Timer* fastPolyInterpTimer = timers.newTimer("FastInterp", "DoWork");
	fastPolyInterpTimer->start();

#if defined PUSH_EXTRA_WORK_TO_WORKER && !defined PRE_COMP_POLYS
	// Calculate v_io(x) and v_mid(x) separately
	FieldElt* denseVio = computeDensePolyIO(V);
	FieldElt* denseVmid = computeDensePolyMid(V);
#else 
	FieldElt* denseV = computeDensePoly(V);
#endif

	// Calculate v(x), w(x), and y(x) in terms of evaluations at targetRoots
	FieldElt* denseW = computeDensePoly(W);
	FieldElt* denseY = computeDensePoly(Y);

	// Now interpolate them to convert to coefficient representation
	// (i.e., find v_i, s.t. v(x) = \sum v_i * x^i)

#if defined PUSH_EXTRA_WORK_TO_WORKER && !defined PRE_COMP_POLYS
	Poly* Vio = interpolate(pk, denseVio);
	Poly* Vmid = interpolate(pk, denseVmid);
	Poly* V = new Poly(field);
	Poly::add(*Vio, *Vmid, *V);
#else 
	Poly *V = interpolate(pk, denseV);
#endif

	Poly* W = interpolate(pk, denseW);
	Poly* Y = interpolate(pk, denseY);

	// Clean up
#if defined PUSH_EXTRA_WORK_TO_WORKER && !defined PRE_COMP_POLYS
	delete [] denseVio;
	delete [] denseVmid;
#else 
	delete [] denseV;
#endif
	delete [] denseW;
	delete [] denseY;

	pk->polyTree->deleteAllButRoot();
	delete [] pk->denominators;
	pk->denominators = NULL;

	// Now calculate Ht = V*W-Y
	Poly Ht(field);
	Poly::mul(*V, *W, Ht);
	Poly::sub(Ht, *Y, Ht);

	// Finally calculate H = Ht/t
	Poly* T = getTarget(pk);
	Poly H(field), r(field), polyZero(field);
	Poly::zero(polyZero, 1);
	Poly::mod(Ht, *T, H, r);
	assert(r.equals(polyZero));

	delete pk->polyTree;	// Free up the memory, now that we no longer need it
	pk->polyTree = NULL;

	fastPolyInterpTimer->stop();

	///////////  If we're doing NIZK, rerandomize h(x) before we apply it ///////////////////////
	Timer* nizkTimer = NULL;
	FieldElt deltaV, deltaW, deltaVW, deltaY;
	
	if (config.count("nizk")) {	
		nizkTimer = timers.newTimer("NIZKcalc", "DoWork");
		nizkTimer->start();

		// Rerandomize to get h'(x) <- h(x) + W(x)*deltaV + V(x)*deltaW + deltaV*deltaW*t(x) - deltaY
		field->assignRandomElt(&deltaV);
		field->assignRandomElt(&deltaW);
		field->assignRandomElt(&deltaY);
		field->mul(deltaV, deltaW, deltaVW);

		Poly tmp(field);
		Poly::constMul(*W, deltaV, tmp);
		Poly::add(H, tmp, H);	// h <- h(x) + W(x)*deltaV
		Poly::constMul(*V, deltaW, tmp);
		Poly::add(H, tmp, H);	// h += V(x)*deltaW
		Poly::constMul(*T, deltaVW, tmp);
		Poly::add(H, tmp, H);	// h += deltaV*deltaW*t(x)	
		FieldElt* coeff = tmp.getCoefficientArray(0);
		field->copy(deltaY, coeff[0]);
		tmp.setCoefficients(coeff, 0);
		Poly::sub(H, tmp, H);	// h -= deltaY

		nizkTimer->stop();
	}

	///////////  Apply h(x)'s coefficients to the s^i and \alpha*s^i PK values ///////////////////////
	Timer* applyTimer = timers.newTimer("ApplyCoeff", "DoWork");

	// Precompute mult tables for the g^{s^i} terms to speed these calcluations
	MultiExpPrecompTable<LEncodedElt> precompTablePowers;
	MultiExpPrecompTable<LEncodedElt> precompTableAlphaPowers;

	// We don't count this against the cost of apply
	int useCount = 1;
#if defined PUSH_EXTRA_WORK_TO_WORKER && !defined PRE_COMP_POLYS
	useCount = 3;
#endif
	TIME(encoding->precomputeMultiPowers(precompTablePowers, pk->powers, pk->numPowers, useCount, config["mem"].as<int>(), config.count("fill") != 0), "PreWorkMultiExp", "OneTimeWork");
	
	applyTimer->start();
	applyCoefficients(precompTablePowers, H.getDegree() + 1, H.coefficients, proof->H);
	applyTimer->stop();

	///////////  Now calculate V(x), W(x), Y(x), etc. in the exponent ///////////////////////
#ifdef PRE_COMP_POLYS
	// Precalculate the v_{mid}_k(x) and y_k(x) terms in the exponent, since the verifier no longer does it for us
	Timer* lagTimer = timers.newTimer("ComputeLi", "OneTimeWork");
	lagTimer->start();
	Poly** lagrangePolys = new Poly*[degree];
	Poly divisor(field, 1);
	field->one(divisor.coefficients[1]);
	FieldElt zero, one;
	field->zero(zero);
	field->one(one);
	FieldElt inverseDenom;
	for (int i = 0; i < degree; i++) {
		lagrangePolys[i] = new Poly(field);		
		field->sub(zero, targetRoots[i], divisor.coefficients[0]);	// divisor(x) = x - r_i
		field->polyMod(*T, divisor, *lagrangePolys[i], r);
		assert(field->polyIsZero(r));

		field->div(one, pk->denominators[i], inverseDenom);
		field->polyConstMul(*lagrangePolys[i], inverseDenom, *lagrangePolys[i]);
	}	
	lagTimer->stop();
	Timer* applyLagTimer = timers.newTimer("ApplyLi", "OneTimeWork");
	applyLagTimer->start();
	LEncodedElt* encodedLagrangePolys = new LEncodedElt[degree];
	LEncodedElt* encodedAlphaLagrangePolys = new LEncodedElt[degree];
	for (int i = 0; i < degree; i++) {
		applyCoefficients(lagrangePolys[i]->getDegree() + 1, pk->powers, lagrangePolys[i]->coefficients, encodedLagrangePolys[i]);
		applyCoefficients(lagrangePolys[i]->getDegree() + 1, pk->alphaPowers, lagrangePolys[i]->coefficients, encodedAlphaLagrangePolys[i]);
	}
	applyLagTimer->stop();
	Timer* applyVkTimer = timers.newTimer("ApplyVkYk", "OneTimeWork");
	applyVkTimer->start();
	applySparsePolysMid(this->V, encodedLagrangePolys, pk->V);
	applySparsePolys(this->Y, encodedLagrangePolys, pk->Y);
	applySparsePolysMid(this->V, encodedAlphaLagrangePolys, pk->alphaV);
	applySparsePolys(this->Y, encodedAlphaLagrangePolys, pk->alphaY);
	applyVkTimer->stop();

	delete [] lagrangePolys;
	delete [] encodedLagrangePolys;
	delete [] encodedAlphaLagrangePolys;
#endif

	applyTimer->start();
#if defined PUSH_EXTRA_WORK_TO_WORKER && !defined PRE_COMP_POLYS
	// Build V_mid(X) directly from the s^i powers
	applyCoefficients(precompTablePowers, Vmid->getDegree() + 1, Vmid->coefficients, proof->V);
	applyCoefficients(precompTablePowers, Y->getDegree() + 1,    Y->coefficients,    proof->Y);
#else 
	applyCircuitCoefficients(config, pk->V, proof->V, true, applyTimer);	
	applyCircuitCoefficients(config, pk->Y, proof->Y, false, applyTimer); 
#endif		
	encoding->deleteMultiTables(precompTablePowers);

	applyCircuitCoefficients(config, pk->W, proof->W, false, applyTimer); 

	// Apply the alpha powers
	TIME(encoding->precomputeMultiPowers(precompTableAlphaPowers, pk->alphaPowers, pk->numPowers, useCount, config["mem"].as<int>(), config.count("fill") != 0), "PreWorkMultiExp", "OneTimeWork");
	applyCoefficients(precompTableAlphaPowers, H.getDegree() + 1, H.coefficients, proof->alphaH);
#if defined PUSH_EXTRA_WORK_TO_WORKER && !defined PRE_COMP_POLYS
	applyCoefficients(precompTableAlphaPowers, Vmid->getDegree() + 1, Vmid->coefficients, proof->alphaV);
	applyCoefficients(precompTableAlphaPowers, Y->getDegree() + 1,    Y->coefficients,    proof->alphaY);
#else 
	applyCircuitCoefficients(config, pk->alphaV, proof->alphaV, true, applyTimer);
	applyCircuitCoefficients(config, pk->alphaY, proof->alphaY, false, applyTimer);	
#endif
	encoding->deleteMultiTables(precompTableAlphaPowers);

	applyCircuitCoefficients(config, pk->alphaW, proof->alphaW, false, applyTimer);

	// Calculate Encoding(\beta_v * v_mid(s) + \beta_w * w(s) + beta_y*y(s))
	// TODO: Do these all as one big multi-eval -- might save some time
	LEncodedElt ltmpV, ltmpW, ltmpY;
	applyCircuitCoefficients(config, pk->betaV, ltmpV, true, applyTimer);				// ltmpV <- Encoding(betaV * V(s))
	applyCircuitCoefficients(config, pk->betaW, ltmpW, false, applyTimer);			// ltmpW <- Encoding(betaW * W(s)) = \sum a_i * Encoding(betaW*w_i(s))
	applyCircuitCoefficients(config, pk->betaY, ltmpY, false, applyTimer);			// ltmpY <- Encoding(betaY * Y(s)) = \sum a_i * Encoding(betaY*y_i(s))

	//encoding->copy(ltmpV, proof->beta);
	//encoding->add(ltmpY, ltmpV, proof->beta);						// proof->beta = Encoding(betaV * V(s) + betaW * W(s))
	encoding->add(ltmpV, ltmpW, proof->beta);						// proof->beta = Encoding(betaV * V(s) + betaW * W(s))
	encoding->add(proof->beta, ltmpY, proof->beta);			// proof->beta += Encoding(betaY * Y(s))
	applyTimer->stop();		

#if 0
	// Beta check debugging
	LEncodedElt gone, gzero;
	encoding->zero(gzero);
	encoding->one(gone);

	EncodedProduct betaGammaV, vGamma;
	encoding->mul(ltmpV, *pk->gammaR, betaGammaV);
	encoding->mul(proof->V, *pk->betaVgamma, vGamma);
	assert(encoding->equals(betaGammaV, vGamma));

	EncodedProduct suspect0;
	encoding->add(betaGammaV, betaGammaV, suspect0);
	assert(encoding->equals(betaGammaV, suspect0));

	EncodedProduct betaGammaW, wGamma;
	encoding->mul(ltmpW, *pk->gammaR, betaGammaW);
	encoding->mul(*pk->betaWgamma, proof->W, wGamma);
	assert(encoding->equals(betaGammaW, wGamma));

	EncodedProduct betaGammaY, yGamma;
	encoding->mul(ltmpY, *pk->gammaR, betaGammaY);
	encoding->mul(proof->Y, *pk->betaYgamma, yGamma);
	assert(encoding->equals(betaGammaY, yGamma));
	
	EncodedProduct quantA;
	encoding->add(betaGammaV, betaGammaW, quantA);
	encoding->add(quantA, betaGammaY, quantA);

	EncodedProduct betaGammaSumB;
	encoding->add(vGamma, wGamma, betaGammaSumB);						// B uses proof->V, proof-W
	encoding->add(betaGammaSumB, yGamma, betaGammaSumB);

	// Compute (beta_v*gamma)*V_mid, and same for W, Y 
	EncodedProduct newBetaGammaSumC;
	encoding->mul(proof->beta, *pk->gammaR, newBetaGammaSumC);		// C uses proof->beta = beta_v*V + beta_w*W

	assert(encoding->equals(quantA, betaGammaSumB));			// Does not assert
	assert(encoding->equals(quantA, newBetaGammaSumC));		// Asserts
	assert(encoding->equals(betaGammaSumB, newBetaGammaSumC));  // Asserts

#endif	

	///////////  If necessary, rerandomize V(x), W(x), Y(x), etc. in the exponent ///////////////////////
	if (config.count("nizk")) {
		nizkTimer->start();

		// Rerandomize V(x), W(x), Y(X)
		LEncodedElt deltaVt, deltaWt, deltaYt;
		REncodedElt rdeltaWt;
		encoding->mul(*pk->LtAtS, deltaV, deltaVt);
		encoding->mul(*pk->LtAtS, deltaW, deltaWt);
		encoding->mul(*pk->RtAtS, deltaW, rdeltaWt);
		encoding->mul(*pk->LtAtS, deltaY, deltaYt);
		encoding->add(proof->V, deltaVt, proof->V);
		encoding->add(proof->W, rdeltaWt, proof->W);
		encoding->add(proof->Y, deltaYt, proof->Y);

		// Rerandomize \alpha*V(x) and \alpha*W(x)
		encoding->mul(*pk->LalphaTatS, deltaV, deltaVt);
		encoding->mul(*pk->RalphaTatS, deltaW, rdeltaWt);
		encoding->mul(*pk->LalphaTatS, deltaY, deltaYt);
		encoding->add(proof->alphaV, deltaVt, proof->alphaV);
		encoding->add(proof->alphaW, rdeltaWt, proof->alphaW);
		encoding->add(proof->alphaY, deltaYt, proof->alphaY);

		// Rerandomize \beta_v*V(x) and \beta_w*W(x)
		encoding->mul(*pk->LbetaTatS, deltaV, deltaVt);		
		encoding->mul(*pk->LbetaTatS, deltaW, deltaWt);
		encoding->mul(*pk->betaYTatS, deltaY, deltaYt);
		encoding->add(proof->beta, deltaVt, proof->beta);
		encoding->add(proof->beta, deltaWt, proof->beta);
		encoding->add(proof->beta, deltaYt, proof->beta);
		
		nizkTimer->stop();
	}

#if defined PUSH_EXTRA_WORK_TO_WORKER && !defined PRE_COMP_POLYS
	delete Vio;
	delete Vmid;
#endif
	delete V;
	delete W;
	delete Y;	

	return proof;
}

#else // !USE_OLD_PROTO

/***************** New Version of DoWork *************************/
Proof* QAP::doWork(PublicKey* pubKey, variables_map& config) {
	QapPublicKey* pk = (QapPublicKey*)pubKey;
	Proof* proof = new Proof((int)circuit->inputs.size() - (int)circuit->witness.size(), (int)circuit->outputs.size());

	// Sometimes the verifier is nice and builds these for us.  Sometimes he isn't so nice.
	if (pk->polyTree == NULL) {
		TIME(pk->polyTree = new PolyTree(field, targetRoots, numTargetRoots), "PreWorkPolyTree", "OneTimeWork");
	}

	// Sometimes the verifier is nice and builds these for us.  Sometimes he isn't so nice.
	if (pk->denominators == NULL) {
		TIME(pk->denominators = createLagrangeDenominators(), "PreWorkDenomin", "OneTimeWork");
	}

	// Copy over the inputs and outputs
	for (int i = 0; i < (int) circuit->inputs.size() - (int) circuit->witness.size(); i++) {
		field->copy(circuit->inputs[i]->value, proof->inputs[i]);
	}

	for (int i = 0; i < circuit->outputs.size(); i++) {
		field->copy(circuit->outputs[i]->value, proof->outputs[i]);
	}

	///////////  First, derive h(x): t(x)h(x) = v(x)w(x) - y(x) ///////////////////////
	Timer* fastPolyInterpTimer = timers.newTimer("FastInterp", "DoWork");
	fastPolyInterpTimer->start();

	// Calculate v(x), w(x), and y(x) in terms of evaluations at targetRoots
	FieldElt* denseV = computeDensePoly(V);
	FieldElt* denseW = computeDensePoly(W);
	FieldElt* denseY = computeDensePoly(Y);

	// Now interpolate them to convert to coefficient representation
	// (i.e., find v_i, s.t. v(x) = \sum v_i * x^i)
	Poly* Vpoly = interpolate(pk, denseV);
	Poly* Wpoly = interpolate(pk, denseW);
	Poly* Ypoly = interpolate(pk, denseY);
		
	delete [] denseV;
	delete [] denseW;
	delete [] denseY;
	pk->polyTree->deleteAllButRoot();
	delete [] pk->denominators;
	pk->denominators = NULL;

	// Now calculate Ht = V*W-Y
	Poly Ht(field);
	Poly::mul(*Vpoly, *Wpoly, Ht);
	Poly::sub(Ht, *Ypoly, Ht);

	// Finally calculate H = Ht/t
	Poly* T = getTarget(pk);
	Poly H(field), r(field), polyZero(field);
	Poly::zero(polyZero, 1);
	Poly::mod(Ht, *T, H, r);
	assert(r.equals(polyZero));
	if (!r.equals(polyZero)) {
		printf("Error: Prover got non-zero remainder when dividing by t(x)!\n");
	}
	fastPolyInterpTimer->stop();

	///////////  If we're doing NIZK, rerandomize h(x) before we apply it ///////////////////////
	Timer* nizkTimer = NULL;
	FieldElt deltaV, deltaW, deltaVW, deltaY;

	// Use to sync the one-pass mode with the dowork mode
	//field->assignRandomElt(&deltaV);
	//field->assignRandomElt(&deltaV);
	//field->assignRandomElt(&deltaV);
	//field->assignRandomElt(&deltaV);
	//field->assignRandomElt(&deltaV);
	//field->assignRandomElt(&deltaV);
	//field->assignRandomElt(&deltaV);
	//field->assignRandomElt(&deltaV);
	
	if (config.count("nizk")) {	
		nizkTimer = timers.newTimer("NIZKcalc", "DoWork");
		nizkTimer->start();

		// Rerandomize to get h'(x) <- h(x) + W(x)*deltaV + V(x)*deltaW + deltaV*deltaW*t(x) - deltaY
		field->assignRandomElt(&deltaV);
		field->assignRandomElt(&deltaW);
		field->assignRandomElt(&deltaY);		
		field->mul(deltaV, deltaW, deltaVW);

		Poly tmp(field);
		Poly::constMul(*Wpoly, deltaV, tmp);
		Poly::add(H, tmp, H);	// h <- h(x) + W(x)*deltaV
		Poly::constMul(*Vpoly, deltaW, tmp);
		Poly::add(H, tmp, H);	// h += V(x)*deltaW
		Poly::constMul(*T, deltaVW, tmp);
		Poly::add(H, tmp, H);	// h += deltaV*deltaW*t(x)	
		FieldElt* coeff = tmp.getCoefficientArray(0);
		field->copy(deltaY, coeff[0]);
		tmp.setCoefficients(coeff, 0);
		Poly::sub(H, tmp, H);	// h -= deltaY

		nizkTimer->stop();
	}

	delete pk->polyTree;	// Free up the memory, now that we no longer need it	
	pk->polyTree = NULL;
	delete Vpoly;
	delete Wpoly;
	delete Ypoly;	

	///////////  Apply h(x)'s coefficients to the s^i PK values ///////////////////////
	Timer* applyTimer = timers.newTimer("ApplyCoeff", "DoWork");

	// Precompute mult tables for the g^{s^i} terms to speed these calcluations
	MultiExpPrecompTable<LEncodedElt> precompTablePowers;

	// We don't count this against the cost of apply
	TIME(encoding->precomputeMultiPowers(precompTablePowers, pk->powers, pk->numPowers, 1, config["mem"].as<int>(), config.count("fill") != 0), "PreWorkMultiExp", "OneTimeWork");
	
	applyTimer->start();
	applyCoefficients(precompTablePowers, H.getDegree() + 1, H.coefficients, proof->H);
	applyTimer->stop();

	encoding->deleteMultiTables(precompTablePowers);

	///////////  Now calculate V(x), W(x), Y(x), etc. in the exponent ///////////////////////
	applyTimer->start();

#ifndef CHECK_NON_ZERO_IO	
	applyCircuitCoefficients(config, pk->Vpolys, proof->V, true,  applyTimer);	
	applyCircuitCoefficients(config, pk->Wpolys, proof->W, false, applyTimer); 
	applyCircuitCoefficients(config, pk->Ypolys, proof->Y, false, applyTimer); 	
	
	// Apply the alpha powers
	applyCircuitCoefficients(config, pk->alphaVpolys, proof->alphaV, true,  applyTimer);
	applyCircuitCoefficients(config, pk->alphaWpolys, proof->alphaW, false, applyTimer);
	applyCircuitCoefficients(config, pk->alphaYpolys, proof->alphaY, false, applyTimer);

	// Calculate Encoding(\beta * rV * v_mid(s) + \beta * rW * w(s) + beta * rY * y(s))	
	applyCircuitCoefficients(config, pk->betaVWYpolys, proof->beta, false, applyTimer);		

#else // defined CHECK_NON_ZERO_IO
	applyCircuitCoefficients(config, pk->Vpolys, proof->V, false,  applyTimer);	
	applyCircuitCoefficients(config, pk->Wpolys, proof->W, true, applyTimer); 
	applyCircuitCoefficients(config, pk->Ypolys, proof->Y, false, applyTimer); 	
	
	// Apply the alpha powers
	applyCircuitCoefficients(config, pk->alphaVpolys, proof->alphaV, false,  applyTimer);
	applyCircuitCoefficients(config, pk->alphaWpolys, proof->alphaW, false, applyTimer);
	applyCircuitCoefficients(config, pk->alphaYpolys, proof->alphaY, false, applyTimer);

	// Calculate Encoding(\beta * rV * v_mid(s) + \beta * rW * w(s) + beta * rY * y(s))	
	applyAllCircuitCoefficients(config, pk->betaVWYpolys, proof->beta, applyTimer);	
#endif // CHECK_NON_ZERO_IO		

	applyTimer->stop();		

	///////////  If necessary, rerandomize V(x), W(x), Y(x), etc. in the exponent ///////////////////////
	if (config.count("nizk")) {
		nizkTimer->start();

		// Rerandomize V(x), W(x), Y(X)
		LEncodedElt deltaVt, deltaWt, deltaYt;
		REncodedElt rdeltaWt;
		encoding->mul(*pk->LVtAtS, deltaV, deltaVt);
		encoding->mul(*pk->RWtAtS, deltaW, rdeltaWt);
		encoding->mul(*pk->LYtAtS, deltaY, deltaYt);
		encoding->add(proof->V, deltaVt, proof->V);
		encoding->add(proof->W, rdeltaWt, proof->W);
		encoding->add(proof->Y, deltaYt, proof->Y);

		// Rerandomize \alpha*V(x), \alpha*W(x), \alpha*Y(x)
		encoding->mul(*pk->LalphaVTatS, deltaV, deltaVt);
		encoding->mul(*pk->LalphaWTatS, deltaW, deltaWt);
		encoding->mul(*pk->LalphaYTatS, deltaY, deltaYt);
		encoding->add(proof->alphaV, deltaVt, proof->alphaV);
		encoding->add(proof->alphaW, deltaWt, proof->alphaW);
		encoding->add(proof->alphaY, deltaYt, proof->alphaY);

		// Rerandomize \beta_v*V(x) and \beta_w*W(x)
		encoding->mul(*pk->LbetaVTatS, deltaV, deltaVt);		
		encoding->mul(*pk->LbetaWTatS, deltaW, deltaWt);
		encoding->mul(*pk->LbetaYTatS, deltaY, deltaYt);
		encoding->add(proof->beta, deltaVt, proof->beta);
		encoding->add(proof->beta, deltaWt, proof->beta);
		encoding->add(proof->beta, deltaYt, proof->beta);
		
		nizkTimer->stop();
	}

	return proof;
}

#endif  // USE_OLD_PROTO

// Check whether base^exp == target
template <typename EncodedEltType>
bool QAP::checkConstEquality(EncodedEltType& base, FieldElt& exp, EncodedEltType& target) {
	EncodedEltType expResult;
	encoding->mul(base, exp, expResult);
	return encoding->equals(expResult, target);
}


/***************** Old Version of Verify *************************/
#ifdef USE_OLD_PROTO

bool QAP::verify(Keys* keys, Proof* proof, variables_map& config) {
	QapPublicKey* pk = (QapPublicKey*)keys->pk;
	QapSecretKey* sk = (QapSecretKey*)keys->sk;

	Timer* ioTimer = timers.newTimer("IO", "Verify");
	ioTimer->start();

	// DV means we can operate directly on the field values
	// Public means we have to operate inside the encoding
	LEncodedElt encodedVio;
	//if (sk->designatedVerifier) {
	if (config.count("dv")) {
		Timer* dVioTimer = timers.newTimer("DvIO", "IO");
		dVioTimer->start();
		// Compute V terms for input and output	
		FieldElt Vio;  
		field->copy(sk->v[0], Vio);	// Start with v_0

		// Inputs
		FieldElt tmp;
		for (int i = 0; i < circuit->inputs.size(); i++) {		
			field->mul(proof->inputs[i], sk->v[i+1], tmp);
			field->add(Vio, tmp, Vio); // V += inputs[i] * sk->v[i+1];		
		}

		// Outputs
		for (int i = 0; i < circuit->outputs.size(); i++) {
			field->mul(proof->outputs[i], sk->v[i + circuit->inputs.size() + 1], tmp);
			field->add(Vio, tmp, Vio); // V += inputs[i] * sk->v[i+1];	
		}

		encoding->encodeSlowest(Vio, encodedVio);	 // Computes the power directly -- no point building a table just for this
		dVioTimer->stop();
	} // else {
	if (config.count("pv")) {
		Timer* pVioTimer = timers.newTimer("PvIO", "IO");
		pVioTimer->start();
		// Compute V terms for input, output, and v_0
		int ioLen = (int) (circuit->inputs.size() + circuit->outputs.size());
		LEncodedElt** bases = new LEncodedElt*[ioLen];
		FieldElt** exponents = new FieldElt*[ioLen];
					
		// Inputs		
		for (int i = 0; i < circuit->inputs.size(); i++) { // Inputs
			bases[i] = &pk->V[i+1];
			exponents[i] = &proof->inputs[i];
		}

		// Outputs
		for (int i = 0; i < circuit->outputs.size(); i++) { // Outputs
			int expIndex = (int) (circuit->inputs.size() + i);
			bases[expIndex] = &pk->V[polyIndex((GateMul2*)circuit->outputs[i]->input)];
			exponents[expIndex] = &proof->outputs[i];			
		}
		
		// Combine I/O terms
		MultiExpPrecompTable<LEncodedElt> ioTables;
		TIME(encoding->precomputeMultiPowers(ioTables, bases, ioLen, 1, config["mem"].as<int>(), config.count("fill") != 0), "PreVerifyIoTables", "IO");
		TIME(encoding->multiMul(ioTables, exponents, ioLen, encodedVio), "MultiMul", "IO");
		encoding->deleteMultiTables(ioTables);

		// Add in v_0
		encoding->add(pk->V[0], encodedVio, encodedVio);	

		delete [] bases;
		delete [] exponents;
		pVioTimer->stop();
	}		
	ioTimer->stop();

	Timer* checkTimer = timers.newTimer("Checks", "Verify");
	checkTimer->start();

	// Combine I/O terms with the V term from the proof
	LEncodedElt encodedV;
	encoding->add(encodedVio, proof->V, encodedV);
	
	// W(x) = w_0(x) + proof->W
	REncodedElt encodedW;
	encoding->add(pk->W[0], proof->W, encodedW);

	// Y(x) = y_0(x) + proof->Y
	LEncodedElt encodedY;
	encoding->add(pk->Y[0], proof->Y, encodedY);

	// Check whether V*W-Y = H * T
	bool quadraticCheck = true;
	REncodedElt rEncodedOne;
	encoding->one(rEncodedOne);	// Enc(1)

	//if (sk->designatedVerifier) {
	if (config.count("dv")) {
		Timer* dvQuad = timers.newTimer("DvQuadCheck", "Checks");
		dvQuad->start();

		LEncodedElt hTimesT;
		encoding->mul(proof->H, sk->target, hTimesT);	// Enc(H(s)*T(s))

		quadraticCheck = quadraticCheck && encoding->mulSubEquals(encodedV, encodedW, encodedY, rEncodedOne, hTimesT, rEncodedOne);
		dvQuad->stop();
	} //else {
	if (config.count("pv")) {
		Timer* pvQuad = timers.newTimer("PvQuadCheck", "Checks");
		pvQuad->start();
		quadraticCheck = quadraticCheck && encoding->mulSubEquals(encodedV, encodedW, encodedY, rEncodedOne, proof->H, *(pk->RtAtS));
		pvQuad->stop();
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

	//if (sk->designatedVerifier) {
	if (config.count("dv")) {
		Timer* dvAlpha = timers.newTimer("DvAlphaCheck", "Checks");
		dvAlpha->start();
		alphaCheck = alphaCheck && checkConstEquality(proof->V, sk->alpha, proof->alphaV);
		alphaCheck = alphaCheck && checkConstEquality(proof->W, sk->alpha, proof->alphaW);
		alphaCheck = alphaCheck && checkConstEquality(proof->Y, sk->alpha, proof->alphaY);
		alphaCheck = alphaCheck && checkConstEquality(proof->H, sk->alpha, proof->alphaH);
		dvAlpha->stop();
	} //else {
	if (config.count("pv")) {
		Timer* pvAlpha = timers.newTimer("PvAlphaCheck", "Checks");
		pvAlpha->start();
		LEncodedElt lEncodedOne;
		encoding->one(lEncodedOne);	// Enc(1)

		alphaCheck = alphaCheck && encoding->mulEquals(proof->V, *(pk->alphaR), proof->alphaV, rEncodedOne);
		alphaCheck = alphaCheck && encoding->mulEquals(*pk->alphaL, proof->W, lEncodedOne, proof->alphaW);
		alphaCheck = alphaCheck && encoding->mulEquals(proof->Y, *(pk->alphaR), proof->alphaY, rEncodedOne);
		alphaCheck = alphaCheck && encoding->mulEquals(proof->H, *(pk->alphaR), proof->alphaH, rEncodedOne);
		pvAlpha->stop();
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
	//if (sk->designatedVerifier) {
	if (config.count("dv")) {
		Timer* dvBeta = timers.newTimer("DvBetaCheck", "Checks");
		dvBeta->start();

		FieldElt betaGammaV, betaGammaW, betaGammaY;

		field->mul(sk->betaV, sk->gamma, betaGammaV);
		field->mul(sk->betaW, sk->gamma, betaGammaW);
		field->mul(sk->betaY, sk->gamma, betaGammaY);

		// We check in two parts.  All of the V and Y terms left on BaseCurve, so we can check directly
		// Sadly, proof->W lives on the TwistCurve, while betaW*W lives on the BaseCurve,
		// so we need two pairings
		
		// Compute gamma*(betaV*V + betaW*W + betaY*Y)
		LEncodedElt gammaZ;
		encoding->mul(proof->beta, sk->gamma, gammaZ);		

		// Compute (beta_v*gamma)*V_mid, and same for W, Y 
		LEncodedElt vBetaGammaV;
		REncodedElt wBetaGammaW;
		LEncodedElt yBetaGammaY;
	
		encoding->mul(proof->V, betaGammaV, vBetaGammaV);
		encoding->mul(proof->W, betaGammaW, wBetaGammaW);
		encoding->mul(proof->Y, betaGammaY, yBetaGammaY);

		// Combine the BaseCurve items (V & Y) to get (gamma*betaV)*V + (gamma*betaY)*Y
		LEncodedElt sum;
		encoding->add(vBetaGammaV, yBetaGammaY, sum);

		// Subtract to get (allegedly) gamma*(betaW*W) 
		LEncodedElt lgammaW;
		encoding->sub(gammaZ, sum, lgammaW);

		// Check that gamma*(betaW*W) == (beta_v*gamma)*W via two pairings
		LEncodedElt lEncodedOne;
		encoding->one(lEncodedOne);	// Enc(1)
		REncodedElt rEncodedOne;
		encoding->one(rEncodedOne);	// Enc(1)

		betaCheck = betaCheck && encoding->mulEquals(lgammaW, rEncodedOne, lEncodedOne, wBetaGammaW);
		dvBeta->stop();
	} //else {
	if (config.count("pv")) {
		Timer* pvBeta = timers.newTimer("PvBetaCheck", "Checks");
		pvBeta->start();

		// Compute gamma*(betaV*V + betaY*Y + betaW*W)
		EncodedProduct gammaZ;
		encoding->mul(proof->beta, *pk->gammaR, gammaZ);	// One pairing
		
		// Compute (beta_v*gamma)*V_mid, and same for W, Y 
		EncodedProduct vBetaGammaV, wBetaGammaW, yBetaGammaY; 
		encoding->mul(proof->V, *pk->betaVgamma, vBetaGammaV); // One pairing
		encoding->mul(*pk->betaWgamma, proof->W, wBetaGammaW); // One pairing
		encoding->mul(proof->Y, *pk->betaYgamma, yBetaGammaY); // One pairing

		// Combine to get (gamma*betaV)*V + (gamma*betaW)*W + (gamma*betaY)*Y
		EncodedProduct sum;
		encoding->add(vBetaGammaV, wBetaGammaW, sum);
		encoding->add(sum, yBetaGammaY, sum);

		// Check that gamma*(betaV*V + betaW*W + betaY*Y) = (gamma*betaV)*V + (gamma*betaW)*W + (gamma*betaY)*Y
		betaCheck = betaCheck && encoding->equals(gammaZ, sum);
		pvBeta->stop();
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

#else !USE_OLD_PROTO

/***************** New Version of Verify *************************/

// Wrapper to deal with the change of interface introduced by the cache warming step
bool QAP::verify(Keys* keys, Proof* proof, variables_map& config) {
	return verify(keys, proof, config, true);
}

bool QAP::verify(Keys* keys, Proof* proof, variables_map& config, bool recordTiming) {
	QapPublicKey* pk = (QapPublicKey*)keys->pk;
	QapSecretKey* sk = (QapSecretKey*)keys->sk;

	Timer* ioTimer = timers.newTimer("IO", "Verify", recordTiming);
	ioTimer->start();

	// DV means we can operate directly on the field values
	// Public means we have to operate inside the encoding
	LEncodedElt encodedVio;
	LEncodedElt encodedAlphaWio;
	LEncodedElt encodedYio;
	//if (sk->designatedVerifier) {
	if (config.count("dv")) {
		Timer* dVioTimer = timers.newTimer("DvIO", "IO", recordTiming);
		dVioTimer->start();
		
#ifndef CHECK_NON_ZERO_IO
		computeDVio(encoding, circuit->inputs.size() + circuit->outputs.size(), &(sk->v[1]), proof->IO, encodedVio);
		/*
		bigctx_t BignumCtx;
		bigctx_t* pbigctx = &BignumCtx;
		memset(pbigctx, 0, sizeof(bigctx_t));

		// Compute the sum over the integers and do the mod at the end
		// Requires sum to have enough space
		digit_t dtmp[5];
		int sumLen = (int) (5 + (circuit->inputs.size() + circuit->outputs.size() - 1)/64 + 1);  
		digit_t* dsum = new digit_t[sumLen];
		memset(dsum, 0, sizeof(digit_t) * sumLen);
		DWORDREG sumDigitLen = 1;
		
		// Inputs
		for (int i = 0; i < circuit->inputs.size(); i++) {		
			BOOL OK = multiply(sk->v[i+1], 4, proof->inputs[i], 1, dtmp, PBIGCTX_PASS);
			assert(OK);
			OK = add_full(dtmp, 5, dsum, sumDigitLen, dsum, sumLen, &sumDigitLen, PBIGCTX_PASS);		
			assert(OK);
		}

		// Outputs
		for (int i = 0; i < circuit->outputs.size(); i++) {
			BOOL OK = multiply(sk->v[i + circuit->inputs.size() + 1], 4, proof->outputs[i], 1, dtmp, PBIGCTX_PASS);			
			OK = add_full(dtmp, 5, dsum, sumDigitLen, dsum, sumLen, &sumDigitLen, PBIGCTX_PASS);		
			assert(OK);
		}

		// Get back into the field
		FieldElt Vio;  
		BOOL OK = to_modular(dsum, sumDigitLen, Vio, encoding->p, PBIGCTX_PASS);		
		encoding->encodeSlowest(Vio, encodedVio);	 // Computes the power directly -- no point building a table just for this
		assert(OK);

		delete dsum;		
		*/
#else // defined CHECK_NON_ZERO_IO
		int verifiedInputSize = (int) circuit->inputs.size() - (int) circuit->witness.size();
		computeDVio(encoding, V, verifiedInputSize, verifiedInputSize, sk->v, proof->inputs, encodedVio);
		computeDVio(encoding, W, verifiedInputSize, verifiedInputSize, sk->alphaWs, proof->inputs, encodedAlphaWio);
		computeDVio(encoding, Y, verifiedInputSize + circuit->outputs.size(), verifiedInputSize, sk->y, proof->io, encodedYio); 
#endif // CHECK_NON_ZERO_IO

		dVioTimer->stop();
	} // else {

#ifdef DEBUG_PV_VS_DV
	LEncodedElt encodedVioCopy;
	LEncodedElt encodedAlphaWioCopy;
	LEncodedElt encodedYioCopy;

	encoding->copy(encodedVio, encodedVioCopy);
	encoding->copy(encodedAlphaWio, encodedAlphaWioCopy);
	encoding->copy(encodedYio, encodedYioCopy);
#endif // DEBUG_PV_VS_DV

	if (config.count("pv")) {
		Timer* pVioTimer = timers.newTimer("PvIO", "IO", recordTiming);
		pVioTimer->start();

#ifndef CHECK_NON_ZERO_IO
		computePVio(config, encoding, circuit, V, circuit->inputs.size() + circuit->outputs.size(), circuit->inputs.size(),
			          pk->Vpolys, proof->io, encodedVio);
		/*
		// Compute V terms for input and output
		int ioLen = (int) (circuit->inputs.size() + circuit->outputs.size());
		LEncodedElt** bases = new LEncodedElt*[ioLen];
		FieldElt** exponents = new FieldElt*[ioLen];
					
		// Inputs		
		for (int i = 0; i < circuit->inputs.size(); i++) { // Inputs
			bases[i] = &pk->Vpolys[i+1];
			exponents[i] = &proof->inputs[i];
		}

		// Outputs
		for (int i = 0; i < circuit->outputs.size(); i++) { // Outputs
			int expIndex = (int) (circuit->inputs.size() + i);
			bases[expIndex] = &pk->Vpolys[polyIndex((GateMul2*)circuit->outputs[i]->input)];
			exponents[expIndex] = &proof->outputs[i];			
		}
		
		// Combine I/O terms
		MultiExpPrecompTable<LEncodedElt> ioTables;
		TIME(encoding->precomputeMultiPowers(ioTables, bases, ioLen, 1, config["mem"].as<int>(), config.count("fill") != 0), "PreVerifyIoTables", "IO");
		TIME(encoding->multiMul(ioTables, exponents, ioLen, encodedVio), "MultiMul", "IO");
		encoding->deleteMultiTables(ioTables);

		// Don't include v_0 -- it's not an input or output!
		// Add in v_0
		//encoding->add(pk->Vpolys[0], encodedVio, encodedVio);	

		delete [] bases;
		delete [] exponents;
		*/
#else // defined CHECK_NON_ZERO_IO
		int verifiedInputSize = (int) circuit->inputs.size() - (int) circuit->witness.size();
		computePVio(config, encoding, circuit, V, verifiedInputSize,  verifiedInputSize, pk->Vpolys, 
								proof->inputs, encodedVio);
		computePVio(config, encoding, circuit, W, verifiedInputSize,  verifiedInputSize, pk->alphaWpolys, 
							  proof->inputs, encodedAlphaWio);
		computePVio(config, encoding, circuit, Y, verifiedInputSize + circuit->outputs.size(), verifiedInputSize, pk->Ypolys, 
								proof->io, encodedYio); 
#endif // CHECK_NON_ZERO_IO
		pVioTimer->stop();
	}		
	ioTimer->stop();

#ifdef DEBUG_PV_VS_DV
	assert(encoding->equals(encodedVio, encodedVioCopy));
	assert(encoding->equals(encodedAlphaWio, encodedAlphaWioCopy));
	assert(encoding->equals(encodedYio, encodedYioCopy));
#endif // DEBUG_PV_VS_DV

	Timer* checkTimer = timers.newTimer("Checks", "Verify", recordTiming);
	checkTimer->start();

	// Combine I/O terms with the V term from the proof
	LEncodedElt encodedV, encodedVplusIO;
	encoding->add(encodedVio, proof->V, encodedVplusIO);		// encodedVplusIO does not include v_0(x)
	encoding->add(encodedVplusIO, pk->Vpolys[0], encodedV); // encodedV includes everything (including v_0)
	
	// W(x) = w_0(x) + proof->W
	REncodedElt encodedW;
	LEncodedElt fullAlphaW;
	encoding->add(pk->Wpolys[0], proof->W, encodedW);
#ifdef CHECK_NON_ZERO_IO
	// Add the input term we calculated for alphaW
	encoding->add(encodedAlphaWio, proof->alphaW, fullAlphaW);
#else // !def CHECK_NON_ZERO_IO
	encoding->copy(proof->alphaW, fullAlphaW);
#endif // CHECK_NON_ZERO_IO

	// Y(x) = y_0(x) + proof->Y
	LEncodedElt encodedY, encodedYplusIO;
#ifndef CHECK_NON_ZERO_IO	
	encoding->add(pk->Ypolys[0], proof->Y, encodedY);
#else // defined CHECK_NON_ZERO_IO
	// Add the output term we calculated for Y
	encoding->add(proof->Y, encodedYio, encodedYplusIO);
	encoding->add(pk->Ypolys[0], encodedYplusIO, encodedY);
#endif // CHECK_NON_ZERO_IO

	// Check whether V*W-Y = H * T
	bool quadraticCheck = true;
	REncodedElt rEncodedOne;
	encoding->one(rEncodedOne);	// Enc(1)

	//if (sk->designatedVerifier) {
	if (config.count("dv")) {
		Timer* dvQuad = timers.newTimer("DvQuadCheck", "Checks", recordTiming);
		dvQuad->start();

		FieldElt rYt;
		field->mul(sk->rY, sk->target, rYt);
		LEncodedElt hTimesTrY;
		encoding->mul(proof->H, rYt, hTimesTrY);	// Enc(H(s)*T(s)*rY)

		quadraticCheck = quadraticCheck && encoding->mulSubEquals(encodedV, encodedW, encodedY, rEncodedOne, hTimesTrY, rEncodedOne);
		dvQuad->stop();
	} //else {
	if (config.count("pv")) {
		Timer* pvQuad = timers.newTimer("PvQuadCheck", "Checks", recordTiming);
		pvQuad->start();
		quadraticCheck = quadraticCheck && encoding->mulSubEquals(encodedV, encodedW, encodedY, rEncodedOne, proof->H, *(pk->RtAtS));
		pvQuad->stop();
	}
#ifdef VERBOSE
//	assert(quadraticCheck);
	if (quadraticCheck) { 
		printf("\tPassed quadratic check!\n");
	} else {
		printf("\tFailed quadratic check!\n");
	}
#endif // VERBOSE

	// Check that the base terms match the alpha terms
	bool alphaCheck = true;

	//if (sk->designatedVerifier) {
	if (config.count("dv")) {
		Timer* dvAlpha = timers.newTimer("DvAlphaCheck", "Checks", recordTiming);
		dvAlpha->start();
		alphaCheck = alphaCheck && checkConstEquality(proof->V, sk->alphaV, proof->alphaV);
		alphaCheck = alphaCheck && encoding->mulEquals(*pk->alphaW, proof->W, fullAlphaW, rEncodedOne);			// Need a pairing to check now, since alphaW on base, W on twist
		//alphaCheck = alphaCheck && checkConstEquality(proof->W, sk->alphaW, fullAlphaW);	
		alphaCheck = alphaCheck && checkConstEquality(proof->Y, sk->alphaY, proof->alphaY);		
		dvAlpha->stop();
	} //else {
	if (config.count("pv")) {
		Timer* pvAlpha = timers.newTimer("PvAlphaCheck", "Checks", recordTiming);
		pvAlpha->start();

		alphaCheck = alphaCheck && encoding->mulEquals(proof->V, *(pk->alphaV), proof->alphaV, rEncodedOne);
		alphaCheck = alphaCheck && encoding->mulEquals(*pk->alphaW, proof->W, fullAlphaW, rEncodedOne);
		alphaCheck = alphaCheck && encoding->mulEquals(proof->Y, *(pk->alphaY), proof->alphaY, rEncodedOne);		
		pvAlpha->stop();
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
	//if (sk->designatedVerifier) {
	if (config.count("dv")) {
		Timer* dvBeta = timers.newTimer("DvBetaCheck", "Checks", recordTiming);
		dvBeta->start();
		
		// We have to do this check via two pairings, 
		// since V, Y, and beta*(V+W+Y) live on the base curve,
		// but W lives on the twist curve

		// Compute gamma*(betaV*V + betaW*W + betaY*Y)
		LEncodedElt gammaZ;
		encoding->mul(proof->beta, sk->gamma, gammaZ);			// = Enc( [beta*(V(s)+W(s)+Y(s))] * gamma)

		FieldElt betaGamma;
		field->mul(sk->beta, sk->gamma, betaGamma);					

		LEncodedElt encodedVY, betaGammaVY;
#ifndef CHECK_NON_ZERO_IO
		encoding->add(encodedVplusIO, proof->Y, encodedVY);				// = Enc(V(s) + Y(s))
#else // defined CHECK_NON_ZERO_IO
		encoding->add(encodedVplusIO, encodedYplusIO, encodedVY);				// = Enc(V(s) + Y(s))
#endif
		encoding->mul(encodedVY, betaGamma, betaGammaVY);		// = Enc(beta*gamma*(V(s) + Y(s))

		REncodedElt betaGammaW;
		encoding->mul(proof->W, betaGamma, betaGammaW);			// = Enc(beta*gamma*(W(s))

		// To do the comparison with only two pairings, calculate:
		// Enc( [beta*(V(s)+W(s)+Y(s))] * gamma) - Enc(beta*gamma*(V(s) + Y(s)) on the base curve
		LEncodedElt diff;
		encoding->sub(gammaZ, betaGammaVY, diff);

		// After pairing, diff should be e(g,g)^{beta*gamma*W(s)}.  Check that by pairing Enc(beta*gamma*(W(s))
		LEncodedElt lEncodedOne;
		encoding->one(lEncodedOne);
		betaCheck = betaCheck && encoding->mulEquals(diff, rEncodedOne, lEncodedOne, betaGammaW);

		dvBeta->stop();
	} //else {
	if (config.count("pv")) {
		Timer* pvBeta = timers.newTimer("PvBetaCheck", "Checks", recordTiming);
		pvBeta->start();

		// Compute gamma*(betaV*V + betaW*W + betaY*Y)
		EncodedProduct gammaZ;
		encoding->mul(proof->beta, *pk->gammaR, gammaZ);			// = Enc( [beta*(V(s)+W(s)+Y(s))] * gamma)
		
		// Compute (beta*gamma)(V + Y)
		LEncodedElt encodedVY;
#ifndef CHECK_NON_ZERO_IO
		encoding->add(encodedVplusIO, proof->Y, encodedVY);		
#else // defined CHECK_NON_ZERO_IO
		encoding->add(encodedVplusIO, encodedYplusIO, encodedVY);		
#endif
		EncodedProduct betaGammaVY;
		encoding->mul(encodedVY, *pk->betaGammaR, betaGammaVY);		// = Enc(beta*gamma*(V(s) + Y(s))

		// Compute (beta*gamma)(W)
		EncodedProduct betaGammaW;
		encoding->mul(*pk->betaGammaL, proof->W, betaGammaW);   // = Enc(beta*gamma*(W(s))

		// Combine to get (beta*gamma)*(V + Y + W)
		EncodedProduct sum;
		encoding->add(betaGammaVY, betaGammaW, sum);

		// Compare
		betaCheck = betaCheck && encoding->equals(gammaZ, sum);

		pvBeta->stop();
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



template <typename EncodedEltType>
void QAP::computeDVio(Encoding* encoding, SparsePolynomial* polys, size_t len, size_t numInputs, FieldElt* secretKeyVals, FieldElt* proofVals, EncodedEltType& result) {
#ifndef DEBUG_ENCODING
#ifndef DEBUG_FAST_DV_IO
		bigctx_t BignumCtx;
		bigctx_t* pbigctx = &BignumCtx;
		memset(pbigctx, 0, sizeof(bigctx_t));

		// Compute the sum over the integers and do the mod at the end
		// Requires sum to have enough space
		digit_t dtmp[8];
		int max_len = (int) len;
		int sumLen = (int) (8 + (max_len - 1)/64 + 1);  
		digit_t* dsum = new digit_t[sumLen];
		memset(dsum, 0, sizeof(digit_t) * sumLen);
		DWORDREG sumDigitLen = 1;
		
		for (int i = 0; i < len; i++) {
			if (i >= numInputs || polys[i+1].size() > 0) {	// Only process non-zero polynomial values
				BOOL OK = multiply(secretKeyVals[i], 4, proofVals[i], 4, dtmp, PBIGCTX_PASS);
				assert(OK);
				OK = add_full(dtmp, 8, dsum, sumDigitLen, dsum, sumLen, &sumDigitLen, PBIGCTX_PASS);		
				assert(OK);
			}
		}

		// Get back into the field
		FieldElt fResult;  
		BOOL OK = to_modular(dsum, sumDigitLen, fResult, encoding->p, PBIGCTX_PASS);		
		encoding->encodeSlowest(fResult, result);	 // Computes the power directly -- no point building a table just for this
		assert(OK);

		delete dsum;
		
		/*
		bigctx_t BignumCtx;
		bigctx_t* pbigctx = &BignumCtx;
		memset(pbigctx, 0, sizeof(bigctx_t));

		// Compute the sum over the integers and do the mod at the end
		// Requires sum to have enough space
		digit_t dtmp[5];
		int max_len = (int) len;
		int sumLen = (int) (5 + (max_len - 1)/64 + 1);  
		digit_t* dsum = new digit_t[sumLen];
		memset(dsum, 0, sizeof(digit_t) * sumLen);
		DWORDREG sumDigitLen = 1;
		
		for (int i = 0; i < len; i++) {
			if (i >= numInputs || polys[i+1].size() > 0) {	// Only process non-zero polynomial values
				BOOL OK = multiply(secretKeyVals[i], 4, proofVals[i], 1, dtmp, PBIGCTX_PASS);
				assert(OK);
				OK = add_full(dtmp, 5, dsum, sumDigitLen, dsum, sumLen, &sumDigitLen, PBIGCTX_PASS);		
				assert(OK);
			}
		}

		// Get back into the field
		FieldElt fResult;  
		BOOL OK = to_modular(dsum, sumDigitLen, fResult, encoding->p, PBIGCTX_PASS);		
		encoding->encodeSlowest(fResult, result);	 // Computes the power directly -- no point building a table just for this
		assert(OK);

		delete dsum;
		*/
#else // def DEBUG_FAST_DV_IO		
		FieldElt tmp, fResult;
		field->zero(fResult);
		for (int i = 0; i < len; i++) {
			if (i >= numInputs || polys[i+1].size() > 0) {	// Only process non-zero polynomial values
				field->mul(secretKeyVals[i], proofVals[i], tmp);
				field->add(fResult, tmp, fResult);				
			}
		}
		encoding->encodeSlowest(fResult, result);
#endif // DEBUG_FAST_DV_IO

#else // DEBUG_ENCODING				
		encoding->zero(result);
		FieldElt tmp;
		for (int i = 0; i < len; i++) {
			if (i >= numInputs || polys[i+1].size() > 0) {	// Only process non-zero polynomial values
				field->mul(secretKeyVals[i], proofVals[i], tmp);
				encoding->add(result, tmp, result);				
			}
		}

		//printf("DV verification not currently supported during encoding debugging\n  High-speed I/O processing only works for regular encoding.\n");
#endif
}

template <typename EncodedEltType>
void QAP::computePVio(variables_map& config, Encoding* encoding, Circuit* circuit, SparsePolynomial* polys, size_t ioLen, size_t outStart, 
											EncodedEltType* pubKeyVals, FieldElt* proofVals, EncodedEltType& result) {
		LEncodedElt** bases = new LEncodedElt*[ioLen];
		FieldElt** exponents = new FieldElt*[ioLen];
		
		int nonZeroIndex = 0;
		for (int i = 0; i < ioLen; i++) { // Inputs
			if (i < outStart) {
#ifndef CHECK_NON_ZERO_IO
				bases[nonZeroIndex] = &pubKeyVals[i+1];
				exponents[nonZeroIndex] = &proofVals[i];
				nonZeroIndex++;
#else // defined CHECK_NON_ZERO_IO
				if (polys[i+1].size() > 0) {
					bases[nonZeroIndex] = &pubKeyVals[i+1];
					exponents[nonZeroIndex] = &proofVals[i];
					nonZeroIndex++;
				}
#endif	// CHECK_NON_ZERO_IO
			} else {				
				bases[nonZeroIndex] = &pubKeyVals[polyIndex((GateMul2*)circuit->outputs[i - outStart]->input)];
				exponents[nonZeroIndex] = &proofVals[i];
				nonZeroIndex++;
			}
		}
		
		// Combine I/O terms
		MultiExpPrecompTable<LEncodedElt> ioTables;
		TIME(encoding->precomputeMultiPowers(ioTables, bases, nonZeroIndex, 1, config["mem"].as<int>(), config.count("fill") != 0), "PreVerifyIoTables", "IO");
		TIME(encoding->multiMul(ioTables, exponents, nonZeroIndex, result), "MultiMul", "IO");
		encoding->deleteMultiTables(ioTables);

		delete [] bases;
		delete [] exponents;
}

#endif // USE_OLD_PROTO
