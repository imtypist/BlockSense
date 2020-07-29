#include "Keys.h"
#include <stdio.h>
#include <assert.h>


#ifdef USE_OLD_PROTO

/////////////////////////// Old Keys //////////////////////////////////////


PublicKey::PublicKey() {
	this->numPowers = -1;
	this->numPolys = -1;

	powers = NULL;
	alphaPowers = NULL;

	powers = NULL;
	alphaPowers = NULL;

	alphaV = NULL;
	alphaW = NULL;

	betaV = NULL;
	betaW = NULL;

	alphaL = NULL;
	alphaR = NULL;
	gammaR = NULL;

	betaVgamma = NULL;
	betaWgamma = NULL;

	RtAtS = NULL;
	
	LtAtS = NULL;
	LalphaTatS = NULL;
	RalphaTatS = NULL;
	LbetaTatS = NULL;

	polyTree = NULL;
	denominators = NULL;
}					

void PublicKey::initialize(int numPowers, int numPolys, bool isNIZK) {
	this->numPowers = numPowers;
	this->numPolys = numPolys;
	this->isNIZK = isNIZK;

	powers = new LEncodedElt[numPowers];
	alphaPowers = new LEncodedElt[numPowers];

	V = new LEncodedElt[numPolys];
	W = new REncodedElt[numPolys];

	alphaV = new LEncodedElt[numPolys];
	alphaW = new REncodedElt[numPolys];

	betaV = new LEncodedElt[numPolys];
	betaW = new LEncodedElt[numPolys];

	alphaL = (LEncodedElt*) new LEncodedElt;
	alphaR = (REncodedElt*) new REncodedElt;
	gammaR = (REncodedElt*) new REncodedElt;
	betaVgamma = (REncodedElt*) new REncodedElt;
	betaWgamma = (LEncodedElt*) new LEncodedElt;

	RtAtS = (REncodedElt*) new REncodedElt;

	LtAtS = NULL;
	LalphaTatS = NULL;
	RalphaTatS = NULL;
	LbetaTatS = NULL;

	polyTree = NULL;
	denominators = NULL;
}

PublicKey::PublicKey(int numPowers, int numPolys, bool isNIZK) {
	initialize(numPowers, numPolys, isNIZK);
}

void PublicKey::cleanup() {
	delete [] powers;
	delete [] alphaPowers;

	delete [] V;
	delete [] W;

	delete [] alphaV;
	delete [] alphaW;

	delete [] betaV;
	delete [] betaW;

	delete [] alphaL;
	delete [] alphaR;
	delete [] gammaR;
	delete [] betaVgamma;
	delete [] betaWgamma;

	delete [] LtAtS;
	delete [] RtAtS;
	delete [] LalphaTatS;
	delete [] RalphaTatS;
	delete [] LbetaTatS;

	delete polyTree;
	delete [] denominators;
}

PublicKey::~PublicKey() {
	cleanup();
}

// Num bytes needed for this key
int PublicKey::size() { 
	int sum = 0;

	sum += (numPowers * sizeof(LEncodedElt)) * 2;	// powers, alphaPowers
	sum += (numPolys * sizeof(LEncodedElt)) * 2;  // betaV, betaW
	sum += (numPolys * sizeof(REncodedElt)) * 1;  // betaW
	//sum += (numPolys * sizeof(LEncodedElt)) * 4;  // V, alphaV, betaV, betaW		// Prover now calculates the rest
	//sum += (numPolys * sizeof(REncodedElt)) * 2;  // W, alphaW, betaW						// Prover now calculates the rest
	sum += sizeof(LEncodedElt) * 2;	// alphaL, betaWgamma
	sum += sizeof(REncodedElt) * 3;	// alphaR, gammaR, betaVgamma

	if (LtAtS != NULL) {	// Count NIZK values too
		sum += sizeof(LEncodedElt) * 3;	// LtAtS, LalphaTatS, LbetaTatS
		sum += sizeof(REncodedElt) * 1;	// RalphaTatS
	}

	return sum;
}

void PublicKey::serialize(Archive* arc) {
	arc->write(numPowers);
	arc->write(numPolys);
	arc->write(isNIZK);

	// Powers of s^i
	arc->write(powers, numPowers);
	arc->write(alphaPowers, numPowers);

	// Polys evaluated at s
	arc->write(V,      numPolys);
	arc->write(W,      numPolys);
	arc->write(alphaV, numPolys);
	arc->write(alphaW, numPolys);
	arc->write(betaV,  numPolys);
	arc->write(betaW,  numPolys);

	// KEA values
	arc->write(*alphaL);
	arc->write(*alphaR);
	arc->write(*gammaR);
	arc->write(*betaVgamma);
	arc->write(*betaWgamma);

	arc->write(*RtAtS);

	if (isNIZK) {
		arc->write(*LtAtS);
		arc->write(*LalphaTatS);
		arc->write(*RalphaTatS);
		arc->write(*LbetaTatS);
	}
}

void PublicKey::deserialize(Archive* arc) {
	arc->read(numPowers);
	arc->read(numPolys);
	arc->read(isNIZK);

	initialize(numPowers, numPolys, isNIZK);

	// Powers of s^i
	arc->read(powers, numPowers);
	arc->read(alphaPowers, numPowers);

	// Polys evaluated at s
	arc->read(V,      numPolys);
	arc->read(W,      numPolys);
	arc->read(alphaV, numPolys);
	arc->read(alphaW, numPolys);
	arc->read(betaV,  numPolys);
	arc->read(betaW,  numPolys);

	// KEA values
	arc->read(*alphaL);
	arc->read(*alphaR);
	arc->read(*gammaR);
	arc->read(*betaVgamma);
	arc->read(*betaWgamma);

	arc->read(*RtAtS);

	if (isNIZK) {
		arc->read(*LtAtS);
		arc->read(*LalphaTatS);
		arc->read(*RalphaTatS);
		arc->read(*LbetaTatS);
	}
}

///////////////////  Generic Secret Key (also used for QSPs) ////////////////////////////

SecretKey::SecretKey() {
	v = NULL;
}

SecretKey::SecretKey(bool designatedVerifier) {
	this->designatedVerifier = designatedVerifier;
	v = NULL;
}

SecretKey::~SecretKey() {
	delete [] v;
}

// Num bytes needed for this key
int SecretKey::size() { 
	int sum = 0;

	if (!designatedVerifier) {	// No secrets with public verifier
		return 0;
	}

	sum += sizeof(FieldElt) * 5;	// alpha, beta*, gamma, target
	
	if (v != NULL) {
		sum += sizeOfv * sizeof(FieldElt);
	} else {
		sum += (int)(vVals.size() * sizeof(FieldElt));
	}

	return sum;
}


void SecretKey::serialize(Archive* arc) {
	if (!designatedVerifier) {
		return;	// No secret key if we're not doing designated verifier
	}

	arc->write(alpha);
	arc->write(betaV);
	arc->write(betaW);
	arc->write(gamma);
	arc->write(target);

	assert(v == NULL);		// Only planning to use this for QAP

	int numVals = (int) vVals.size();
	arc->write(numVals);
	for (IndexedElts::iterator iter = vVals.begin();
		   iter != vVals.end();
			 iter++) {
		arc->write(iter->first);
		arc->write(iter->second);
	}
}

void SecretKey::deserialize(Archive* arc) {
	designatedVerifier = true;		// We only have a secret key if we're doing designated verifier

	arc->read(alpha);
	arc->read(betaV);
	arc->read(betaW);
	arc->read(gamma);
	arc->read(target);

	int numVals = 0;
	arc->read(numVals);
	for (int i = 0; i < numVals; i++) {
		IndexedElt pair;
		arc->read(pair.first);
		arc->read(pair.second);
		vVals.insert(pair);
	}
}

///////////////////  QAP-specific Keys  ////////////////////////////

QapPublicKey::QapPublicKey() {
	Y = NULL;
	alphaY = NULL;
	betaY = NULL;
	betaYgamma = NULL;
	betaYTatS = NULL;
}

QapPublicKey::QapPublicKey(QAP* qap, int numPowers, int numPolys, bool isNIZK) : PublicKey(numPowers, numPolys, isNIZK) {
	this->qap = qap;

	initialize(numPowers, numPolys);
}

void QapPublicKey::initialize(int numPowers, int numPolys) {
	Y = new LEncodedElt[numPolys];
	alphaY = new LEncodedElt[numPolys];
	betaY = new LEncodedElt[numPolys];

	betaYgamma = (REncodedElt*) new REncodedElt;

	betaYTatS = NULL;
}

QapPublicKey::~QapPublicKey() {
	//cleanup();

	delete [] Y;
	delete [] alphaY;
	delete [] betaY;
	delete [] betaYTatS;
	delete [] betaYgamma;
}

// Num bytes needed for this key
int QapPublicKey::size() { 
	int sum = 0;

	sum += PublicKey::size();
	sum += (numPolys * sizeof(LEncodedElt)) * 3; // Y, alphaY, betaY
	sum += sizeof(REncodedElt);  // betaYgamma
	if (betaYTatS != NULL) {
		sum += sizeof(LEncodedElt);
	}

	return sum;
}

void QapPublicKey::serialize(Archive* arc) {
	PublicKey::serialize(arc);
	arc->write(Y, numPolys);
	arc->write(alphaY, numPolys);
	arc->write(betaY, numPolys);

	arc->write(*betaYgamma);

	if (isNIZK) {
		arc->write(*betaYTatS);
	}
}

void QapPublicKey::deserialize(Archive* arc) {
	PublicKey::deserialize(arc);
	this->initialize(numPowers, numPolys);	// Handle QAP-specific initialization

	arc->read(Y, numPolys);
	arc->read(alphaY, numPolys);
	arc->read(betaY, numPolys);

	arc->read(*betaYgamma);

	if (isNIZK) {
		arc->read(*betaYTatS);
	}
}

QapSecretKey::QapSecretKey() {
	;
}

QapSecretKey::QapSecretKey(bool designatedVerifier) : SecretKey(designatedVerifier) {
}

int QapSecretKey::size() {
	int sum = 0;

	sum += SecretKey::size();
	sum += sizeof(FieldElt);	// betaY

	return sum;
}

void QapSecretKey::serialize(Archive* arc) {
	SecretKey::serialize(arc);
	
	if (!designatedVerifier) {
		return;
	}

	arc->write(betaY);
}

void QapSecretKey::deserialize(Archive* arc) {
	SecretKey::deserialize(arc);
	
	arc->read(betaY);
}



///////////////////  QSP-specific Public Key  ////////////////////////////

QspPublicKey::QspPublicKey(QSP* qsp, int numPowers, int numPolys, bool isNIZK) : PublicKey(numPowers, numPolys, isNIZK) {
	this->qsp = qsp;
}



///////////////////  Container for Key Pairs  ////////////////////////////

Keys::Keys(bool designatedVerifier, QAP* qap, int numPowers, int numPolys, bool isNIZK) {
	sk = new QapSecretKey(designatedVerifier);
	pk = new QapPublicKey(qap, numPowers, numPolys, isNIZK);

	if (designatedVerifier) {
		printf("\nPrivate verification\n");
	} else {
		printf("\nPublic verification\n");
	}
}

Keys::Keys(bool designatedVerifier, QSP* qsp, int numPowers, int numPolys, bool isNIZK) {
	sk = new SecretKey(designatedVerifier);	// Nothing special needed for QSP SK
	pk = new QspPublicKey(qsp, numPowers, numPolys, isNIZK);

	if (designatedVerifier) {
		printf("\nPrivate verification\n");
	} else {
		printf("\nPublic verification\n");
	}
}

Keys::~Keys() {
	delete sk;
	delete pk;
}

void Keys::printSizes(size_t raw) {
	if (raw) {
		printf("pkRawSizeBytes: %d\n", this->pk->size());
		printf("skRawSizeBytes: %d\n", this->sk->size());
	} else {
		printf("Public key: %d bytes\n", this->pk->size());
		printf("Secret key: %d bytes\n", this->sk->size());
	}
}



#else // !USE_OLD_PROTO

/////////////////////////// New Keys //////////////////////////////////////


///////////////////  Generic Public Key  ////////////////////////////

void PublicKey::init() {
	this->numPowers = -1;
	this->numPolys = -1;

	powers = NULL;

	Vpolys = NULL;
	Wpolys = NULL;

	alphaVpolys = NULL;
	alphaWpolys = NULL;

	betaVWYpolys = NULL;

	alphaV = NULL;
	alphaW = NULL;

	gammaR = NULL;

	betaGammaL = NULL;
	betaGammaR = NULL;

	RtAtS = NULL;
	
	LVtAtS = NULL;
	RWtAtS = NULL;
	LalphaVTatS = NULL;
	LalphaWTatS = NULL;
	LbetaVTatS = NULL;
	LbetaWTatS = NULL;

	polyTree = NULL;
	denominators = NULL;
}

PublicKey::PublicKey() {
	init();
}					

void PublicKey::initialize(int numPowers, int numPolys, bool isNIZK, bool designatedVerifier) {
	init();

	this->numPowers = numPowers;
	this->numPolys = numPolys;
	this->isNIZK = isNIZK;
	this->designatedVerifier = designatedVerifier;

	powers = new LEncodedElt[numPowers];

	Vpolys = new LEncodedElt[numPolys];
	Wpolys = new REncodedElt[numPolys];

	alphaVpolys = new LEncodedElt[numPolys];
	alphaWpolys = new LEncodedElt[numPolys];

	betaVWYpolys = new LEncodedElt[numPolys];
		
	if (!designatedVerifier) {  // (Otherwise these are all in the secret key)
		alphaV = (REncodedElt*) new REncodedElt;

		gammaR = (REncodedElt*) new REncodedElt;
		betaGammaL = (LEncodedElt*) new LEncodedElt;
		betaGammaR = (REncodedElt*) new REncodedElt;
	
		RtAtS = (REncodedElt*) new REncodedElt;

		if (isNIZK) {
			LVtAtS = (LEncodedElt*) new LEncodedElt;
			RWtAtS = (REncodedElt*) new REncodedElt;
			LalphaVTatS = (LEncodedElt*) new LEncodedElt;
			LalphaWTatS = (LEncodedElt*) new LEncodedElt;
			LbetaVTatS = (LEncodedElt*) new LEncodedElt;
			LbetaWTatS = (LEncodedElt*) new LEncodedElt;
		} 
	}
	// Still need this, even in DV case, due to twist curve
	alphaW = (LEncodedElt*) new LEncodedElt;

	// Provide a basic default, to catch problems sooner
	if (designatedVerifier || !isNIZK) {
		LVtAtS = NULL;
		RWtAtS = NULL;
		LalphaVTatS = NULL;
		LalphaWTatS = NULL;
		LbetaVTatS = NULL;
		LbetaWTatS = NULL;
	}

	polyTree = NULL;
	denominators = NULL;
}

PublicKey::PublicKey(int numPowers, int numPolys, bool isNIZK, bool designatedVerifier) {
	initialize(numPowers, numPolys, isNIZK, designatedVerifier);
}

void PublicKey::cleanup() {
	delete [] powers;

	delete [] Vpolys;
	delete [] Wpolys;

	delete [] alphaVpolys;
	delete [] alphaWpolys;

	delete [] betaVWYpolys;
	
	delete [] alphaV;
	delete [] alphaW;

	delete [] gammaR;
	delete [] betaGammaL;
	delete [] betaGammaR;

	delete [] LVtAtS;
	delete [] RWtAtS;
	delete [] RtAtS;
	delete [] LalphaVTatS;
	delete [] LalphaWTatS;
	delete [] LbetaVTatS;
	delete [] LbetaWTatS;

	delete polyTree;
	delete [] denominators;
}

PublicKey::~PublicKey() {
	cleanup();
}

// Num bytes needed for this key
int PublicKey::size() { 
	int sum = 0;

	sum += (numPowers * sizeof(LEncodedElt)) * 1;	// powers
	sum += (numPolys * sizeof(LEncodedElt)) * (4);  // V, alphaV, alphaW, betaVWYpolys
	sum += (numPolys * sizeof(REncodedElt)) * 1;  // W
	
	sum += sizeof(LEncodedElt) * 1;	// alphaW, 
	sum += sizeof(REncodedElt) * 4;	// alphaV, gammaR, betaGamma, RtAtS

	if (LVtAtS != NULL) {	// Count NIZK values too
		sum += sizeof(LEncodedElt) * 3;	// LVtAtS, LalphaTatS, LbetaTatS
		sum += sizeof(REncodedElt) * 2;	// RalphaTatS + RWtAtS
	}

	return sum;
}

void PublicKey::serialize(Archive* arc) {
	arc->write(numPowers);
	arc->write(numPolys);
	arc->write(isNIZK);
	arc->write(designatedVerifier);

	// Powers of s^i
	arc->write(powers, numPowers);

	// Polys evaluated at s
	arc->write(Vpolys, numPolys);
	arc->write(Wpolys, numPolys);
	arc->write(alphaVpolys, numPolys);
	arc->write(alphaWpolys, numPolys);
	arc->write(betaVWYpolys, numPolys);

	if (!designatedVerifier) {
		// KEA values
		arc->write(*alphaV);
		arc->write(*gammaR);
		arc->write(*betaGammaL);
		arc->write(*betaGammaR);
	
		arc->write(*RtAtS);

		if (isNIZK) {
			arc->write(*LVtAtS);
			arc->write(*RWtAtS);
			arc->write(*LalphaVTatS);
			arc->write(*LalphaWTatS);
			arc->write(*LbetaVTatS);
			arc->write(*LbetaWTatS);
		}
	}

	// Still need this, even in DV case, due to twist curve
	arc->write(*alphaW);
}

void PublicKey::deserialize(Archive* arc) {
	arc->read(numPowers);
	arc->read(numPolys);
	arc->read(isNIZK);
	arc->read(designatedVerifier);

	initialize(numPowers, numPolys, isNIZK, designatedVerifier);

	// Powers of s^i
	arc->read(powers, numPowers);

	// Polys evaluated at s
	arc->read(Vpolys, numPolys);
	arc->read(Wpolys, numPolys);
	arc->read(alphaVpolys, numPolys);
	arc->read(alphaWpolys, numPolys);
	arc->read(betaVWYpolys, numPolys);

	if (!designatedVerifier) {
		// KEA values
		arc->read(*alphaV);	
		arc->read(*gammaR);
		arc->read(*betaGammaL);
		arc->read(*betaGammaR);

		arc->read(*RtAtS);

		if (isNIZK) {
			arc->read(*LVtAtS);
			arc->read(*RWtAtS);
			arc->read(*LalphaVTatS);
			arc->read(*LalphaWTatS);
			arc->read(*LbetaVTatS);
			arc->read(*LbetaWTatS);
		}
	}
	
	// Still need this, even in DV case, due to twist curve
	arc->read(*alphaW);
}

void PublicKey::printCommon(Encoding* encoding) {
	// Powers of s^i
	printf("Public key with %d powers, %d numPolys\n", numPowers, numPolys);
	for (int i = 0; i < numPowers; i++) {
		printf("\t powers[%d] = ", i);
		encoding->print(powers[i]);
		printf("\n");
	}	

	for (int i = 0; i < numPolys; i++) {
		printf("\t Vpolys[%d] = ", i);
		encoding->print(Vpolys[i]);
		printf("\n");
	}	
	for (int i = 0; i < numPolys; i++) {
		printf("\t Wpolys[%d] = ", i);
		encoding->print(Wpolys[i]);
		printf("\n");
	}	
	for (int i = 0; i < numPolys; i++) {
		printf("\t alphaVpolys[%d] = ", i);
		encoding->print(alphaVpolys[i]);
		printf("\n");
	}	
	for (int i = 0; i < numPolys; i++) {
		printf("\t alphaWpolys[%d] = ", i);
		encoding->print(alphaWpolys[i]);
		printf("\n");
	}
	for (int i = 0; i < numPolys; i++) {
		printf("\t betaVWYpolys[%d] = ", i);
		encoding->print(betaVWYpolys[i]);
		printf("\n");
	}

	// KEA values
	printf("\tKEA values:\n\t");
	encoding->print(*alphaV);				printf("\n\t");
	encoding->print(*alphaV);				printf("\n\t");
	encoding->print(*alphaW);				printf("\n\t");
	encoding->print(*gammaR);				printf("\n\t");
	encoding->print(*betaGammaL);		printf("\n\t");
	encoding->print(*betaGammaR);		printf("\n\t");
	encoding->print(*RtAtS);			  printf("\n\t");

	// Some extra values needed for NIZK	
	if (this->isNIZK) {
		encoding->print(*LVtAtS);				printf("\n\t");
		encoding->print(*RWtAtS);				printf("\n\t");
		encoding->print(*LalphaVTatS);	printf("\n\t");
		encoding->print(*LalphaWTatS);	printf("\n\t");
		encoding->print(*LbetaVTatS);		printf("\n\t");
		encoding->print(*LbetaWTatS);		printf("\n\t");
	}
}

#define CHECK(entry, str) \
	if (!encoding->equals(*(entry), *(other->entry))) { printf("%s differs\n", str); equal = false; }

bool PublicKey::isEqual(Encoding* encoding, PublicKey* other) {
	bool equal = true;
	if (numPowers != other->numPowers) {
		printf("numPowers differ %d %d\n", numPowers, other->numPowers);
		equal = false;
	}
	if (numPolys != other->numPolys) {
		printf("numPolys differ %d %d\n", numPolys, other->numPolys);
		equal = false;
	}
	if (isNIZK != other->isNIZK) {
		printf("NIZK differs\n");
		equal = false;
	}
	if (designatedVerifier != other->designatedVerifier) {
		printf("DV differs\n");
		equal = false;
	}

	for (int i = 0; i < numPowers; i++) {
		if (!encoding->equals(powers[i], other->powers[i])) { 
			printf("powers[%d] differs\n", i);
			equal = false;
		}
	}
	for (int i = 0; i < numPolys; i++) {
		if (!encoding->equals(Vpolys[i], other->Vpolys[i])) { 
			printf("Vpolys[%d] differs\n", i);
			equal = false;
		}
	}
	for (int i = 0; i < numPolys; i++) {
		if (!encoding->equals(Wpolys[i], other->Wpolys[i])) { 
			printf("Wpolys[%d] differs\n", i);
			equal = false;
		}
	}
	for (int i = 0; i < numPolys; i++) {
		if (!encoding->equals(alphaVpolys[i], other->alphaVpolys[i])) { 
			printf("alphaVpolys[%d] differs\n", i);
			equal = false;
		}
	}
	for (int i = 0; i < numPolys; i++) {
		if (!encoding->equals(alphaWpolys[i], other->alphaWpolys[i])) { 
			printf("alphaWpolys[%d] differs\n", i);
			equal = false;
		}
	}
	for (int i = 0; i < numPolys; i++) {
		if (!encoding->equals(betaVWYpolys[i], other->betaVWYpolys[i])) { 
			printf("betaVWYpolys[%d] differs\n", i);
			equal = false;
		}
	}

	if (!designatedVerifier) {
		CHECK(alphaV, "alphaV");
		CHECK(alphaW, "alphaW");
		CHECK(gammaR, "gammaR");
		CHECK(betaGammaL, "betaGammaL");
		CHECK(betaGammaR, "betaGammaR");
		CHECK(RtAtS, "RtAtS");

		if (isNIZK) {
			CHECK(LVtAtS, "LVtAtS");
			CHECK(RWtAtS, "RWtAtS");
			CHECK(LalphaVTatS, "LalphaVTatS");
			CHECK(LalphaWTatS, "LalphaWTatS");
			CHECK(LbetaVTatS, "LbetaVTatS");
			CHECK(LbetaWTatS, "LbetaWTatS");
		}
	}
	return equal;
}

bool PublicKey::equals(Encoding* encoding, PublicKey* other) {
	return isEqual(encoding, other);
}

///////////////////  Generic Secret Key (also used for QSPs) ////////////////////////////

SecretKey::SecretKey() {
	v = NULL;
}

SecretKey::SecretKey(bool designatedVerifier) {
	this->designatedVerifier = designatedVerifier;
	v = NULL;
}

SecretKey::~SecretKey() {
	delete [] v;
}

// Num bytes needed for this key
int SecretKey::size() { 
	int sum = 0;

	if (!designatedVerifier) {	// No secrets with public verifier
		return 0;
	}

	sum += sizeof(FieldElt) * (3+1+1+2+1);	// alpha*3, beta, gamma, rV, rW, target
	
	if (v != NULL) {
		sum += sizeOfv * sizeof(FieldElt);
	} else {
		sum += (int)(vVals.size() * sizeof(FieldElt));
	}

	return sum;
}


void SecretKey::serialize(Archive* arc) {
	if (!designatedVerifier) {
		return;	// No secret key if we're not doing designated verifier
	}

	arc->write(alphaV);
	arc->write(alphaW);
	arc->write(beta);
	arc->write(gamma);
	arc->write(rV);
	arc->write(rW);
	arc->write(target);

	arc->write(this->sizeOfv);
	arc->write(v, this->sizeOfv);

	int numVals = (int) vVals.size();
	arc->write(numVals);
	for (IndexedElts::iterator iter = vVals.begin();
		   iter != vVals.end();
			 iter++) {
		arc->write(iter->first);
		arc->write(iter->second);
	}
}

void SecretKey::deserialize(Archive* arc) {
	designatedVerifier = true;		// We only have a secret key if we're doing designated verifier

	arc->read(alphaV);
	arc->read(alphaW);
	arc->read(beta);
	arc->read(gamma);
	arc->read(rV);
	arc->read(rW);
	arc->read(target);

	arc->read(this->sizeOfv);
	this->v = new FieldElt[this->sizeOfv];
	arc->read(this->v, this->sizeOfv);

	int numVals = 0;
	arc->read(numVals);
	for (int i = 0; i < numVals; i++) {
		IndexedElt pair;
		arc->read(pair.first);
		arc->read(pair.second);
		vVals.insert(pair);
	}
}

///////////////////  QAP-specific Keys  ////////////////////////////

void QapPublicKey::QapInit() {
	Ypolys = NULL;
	alphaYpolys = NULL;
	alphaY = NULL;
	LalphaYTatS = NULL;
	LbetaYTatS = NULL;
}


QapPublicKey::QapPublicKey() {
	QapInit();
}

QapPublicKey::QapPublicKey(QAP* qap, int numPowers, int numPolys, bool isNIZK, bool designatedVerifier) : PublicKey(numPowers, numPolys, isNIZK, designatedVerifier) {
	this->qap = qap;

	QapInit();
	initialize(numPowers, numPolys);	
}

void QapPublicKey::initialize(int numPowers, int numPolys) {
	Ypolys = new LEncodedElt[numPolys];
	alphaYpolys = new LEncodedElt[numPolys];	

	if (!designatedVerifier) {  // (Otherwise it's in the secret key)
		alphaY = (REncodedElt*) new REncodedElt;
	}

	if (isNIZK) {
		LYtAtS = (LEncodedElt*) new LEncodedElt;
		LalphaYTatS = (LEncodedElt*) new LEncodedElt;
		LbetaYTatS = (LEncodedElt*) new LEncodedElt;
	} else {
		LYtAtS = NULL;
		LalphaYTatS = NULL;
		LbetaYTatS = NULL;
	}
}

QapPublicKey::~QapPublicKey() {
	//cleanup();

	delete [] Ypolys;
	delete [] alphaYpolys;
	delete [] alphaY;
	delete [] LYtAtS;
	delete [] LalphaYTatS;
	delete [] LbetaYTatS;
}

// Num bytes needed for this key
int QapPublicKey::size() { 
	int sum = 0;

	sum += PublicKey::size();
	sum += (numPolys * sizeof(LEncodedElt)) * 3; // Y, alphaY, betaY
	sum += sizeof(REncodedElt);  // betaYgamma
	if (LYtAtS != NULL) {
		sum += sizeof(LEncodedElt);
	}
	if (LalphaYTatS != NULL) {
		sum += sizeof(LEncodedElt);
	}
	if (LbetaYTatS != NULL) {
		sum += sizeof(LEncodedElt);
	}	

	return sum;
}

void QapPublicKey::serialize(Archive* arc) {
	PublicKey::serialize(arc);
	arc->write(Ypolys, numPolys);
	arc->write(alphaYpolys, numPolys);

	if (!designatedVerifier) {  
		arc->write(*alphaY);
	}

	if (isNIZK) {
		arc->write(*LYtAtS);
		arc->write(*LalphaYTatS);
		arc->write(*LbetaYTatS);
	}
}

void QapPublicKey::deserialize(Archive* arc) {
	PublicKey::deserialize(arc);
	this->initialize(numPowers, numPolys);	// Handle QAP-specific initialization

	arc->read(Ypolys, numPolys);
	arc->read(alphaYpolys, numPolys);

	if (!designatedVerifier) {  
		arc->read(*alphaY);
	}

	if (isNIZK) {
		arc->read(*LYtAtS);
		arc->read(*LalphaYTatS);
		arc->read(*LbetaYTatS);
	}
}

void QapPublicKey::print(Encoding* encoding) {
	this->printCommon(encoding);
	for (int i = 0; i < numPolys; i++) {
		printf("\t Ypolys[%d] = ", i);
		encoding->print(Ypolys[i]);
		printf("\n");
	}
	for (int i = 0; i < numPolys; i++) {
		printf("\t alphaYpolys[%d] = ", i);
		encoding->print(alphaYpolys[i]);
		printf("\n");
	}

	if (!designatedVerifier) {
		encoding->print(*alphaY);				printf("\n\t");
	}

	if (isNIZK) {
		encoding->print(*LYtAtS);				printf("\n\t");
		encoding->print(*LalphaYTatS);	printf("\n\t");
		encoding->print(*LbetaYTatS);		printf("\n\t");
	}
}

bool QapPublicKey::equals(Encoding* encoding, QapPublicKey* other) {
	bool equal = isEqual(encoding, other);

	for (int i = 0; i < numPolys; i++) {
		if (!encoding->equals(Ypolys[i], other->Ypolys[i])) { 
			printf("Ypolys[%d] differs\n", i);
			equal = false;
		}
	}
	for (int i = 0; i < numPolys; i++) {
		if (!encoding->equals(alphaYpolys[i], other->alphaYpolys[i])) { 
			printf("alphaYpolys[%d] differs\n", i);
			equal = false;
		}
	}

	if (!designatedVerifier) {
		CHECK(alphaY, "alphaY");
		if (isNIZK) {
			CHECK(LYtAtS, "LYtAtS");
			CHECK(LalphaYTatS, "LalphaYTatS");
			CHECK(LbetaYTatS, "LbetaYTatS");
		}
	}
	return equal;
}

QapSecretKey::QapSecretKey() {
	numWs = 0;
	numYs = 0;
	alphaWs = NULL;
	y = NULL;
}

QapSecretKey::QapSecretKey(bool designatedVerifier) : SecretKey(designatedVerifier) {
}

int QapSecretKey::size() {
	int sum = 0;

	sum += SecretKey::size();
	sum += sizeof(FieldElt);	// rY
	sum += sizeof(FieldElt);	// alphaY
	sum += sizeof(FieldElt) * numWs;
	sum += sizeof(FieldElt) * numYs;

	return sum;
}

void QapSecretKey::serialize(Archive* arc) {
	SecretKey::serialize(arc);
	
	if (!designatedVerifier) {
		return;
	}

	arc->write(rY);
	arc->write(alphaY);
	arc->write(alphaWs, numWs);
	arc->write(y, numYs);
}

void QapSecretKey::deserialize(Archive* arc) {
	SecretKey::deserialize(arc);
	
	arc->read(rY);
	arc->read(alphaY);
	arc->read(alphaWs, numWs);
	arc->read(y, numYs);
}



///////////////////  QSP-specific Public Key  ////////////////////////////

QspPublicKey::QspPublicKey(QSP* qsp, int numPowers, int numPolys, bool isNIZK, bool designatedVerifier) : PublicKey(numPowers, numPolys, isNIZK, designatedVerifier) {
	this->qsp = qsp;
}



///////////////////  Container for Key Pairs  ////////////////////////////

Keys::Keys(bool designatedVerifier, QAP* qap, int numPowers, int numPolys, bool isNIZK) {
	sk = new QapSecretKey(designatedVerifier);
	pk = new QapPublicKey(qap, numPowers, numPolys, isNIZK, designatedVerifier);

	if (designatedVerifier) {
		printf("\nPrivate verification\n");
	} else {
		printf("\nPublic verification\n");
	}
}

Keys::Keys(bool designatedVerifier, QSP* qsp, int numPowers, int numPolys, bool isNIZK) {
	sk = new SecretKey(designatedVerifier);	// Nothing special needed for QSP SK
	pk = new QspPublicKey(qsp, numPowers, numPolys, isNIZK, designatedVerifier);

	if (designatedVerifier) {
		printf("\nPrivate verification\n");
	} else {
		printf("\nPublic verification\n");
	}
}

Keys::~Keys() {
	delete sk;
	delete pk;
}

void Keys::printSizes(size_t raw) {
	if (raw) {
		printf("pkRawSizeBytes: %d\n", this->pk->size());
		printf("skRawSizeBytes: %d\n", this->sk->size());
	} else {
		printf("Public key: %d bytes\n", this->pk->size());
		printf("Secret key: %d bytes\n", this->sk->size());
	}
}


#endif // USE_OLD_PROTO