#pragma once

#include "Encoding.h"
#include "Poly.h"
#include "Archive.h"
#include <map>

typedef struct {
	FieldElt elt;
} WrappedElt;

typedef std::map<long long, WrappedElt> IndexedElts;
typedef std::pair<long long, WrappedElt> IndexedElt;

class PublicKey;
class QAP;
class QSP;

#ifdef USE_OLD_PROTO

/////////////////////////// Old Keys //////////////////////////////////////
class PublicKey {
public:
	PublicKey();
	PublicKey(int numPowers, int numPolys, bool isNIZK);

	// Powers of s^i
	LEncodedElt* powers;
	LEncodedElt* alphaPowers;

	// Lots of polys evaluated at s
	LEncodedElt* V;
	REncodedElt* W;

	LEncodedElt* alphaV;
	REncodedElt* alphaW;

	LEncodedElt* betaV;
	LEncodedElt* betaW;	

	// KEA values
	LEncodedElt* alphaL;
	REncodedElt* alphaR;
	REncodedElt* gammaR;
	REncodedElt* betaVgamma;
	LEncodedElt* betaWgamma;

	REncodedElt* RtAtS;	

	// Some extra values needed for NIZK	
	LEncodedElt* LtAtS;
	LEncodedElt* LalphaTatS;
	REncodedElt* RalphaTatS;
	LEncodedElt* LbetaTatS;
	
	PolyTree* polyTree;			// Precompute polynomial tree, with leaves (x-r), for r in target roots, and internal nodes as products of their children
	FieldElt* denominators;		// Pre-computed Lagrange denominators

	// Num bytes needed for this key
	virtual int size();

	int numPowers;
	int numPolys;
	bool isNIZK;

	virtual void serialize(Archive* arc);
	virtual void deserialize(Archive* arc);

	void cleanup();
	virtual ~PublicKey();

protected:
	virtual void initialize(int numPowers, int numPolys, bool isNIZK);
};

class SecretKey {
public:
	SecretKey();
	SecretKey(bool designatedVerifier);
	~SecretKey();

	bool designatedVerifier;	// If this is false, all other fields are invalid

	FieldElt alpha;
	FieldElt betaV;
	FieldElt betaW;
	FieldElt gamma;
	FieldElt target;

	int sizeOfv;
	FieldElt* v;	// Various v polynomials (v_0, v_in, v_out) evaluated at s
	IndexedElts vVals;

	// Num bytes needed for this key
	virtual int size();
	void cleanup();

	virtual void serialize(Archive* arc);
	virtual void deserialize(Archive* arc);
};

class Keys {
public:
	Keys(bool designatedVerifier, QAP* qap, int numPowers, int numPolys, bool isNIZK);
	Keys(bool designatedVerifier, QSP* qsp, int numPowers, int numPolys, bool isNIZK);
	~Keys();

	// Raw = 0 means human readable
	void printSizes(size_t raw);

	SecretKey* sk;
	PublicKey* pk;
};


// Holds all of the QAP-specific key elements
class QapPublicKey : public PublicKey {
public:
	QapPublicKey();
	QapPublicKey(QAP* qap, int numPowers, int numPolys, bool isNIZK);	
	~QapPublicKey();

	QAP* qap;

	// Three sets of polys evaluated at s
	LEncodedElt* Y;
	LEncodedElt* alphaY;
	LEncodedElt* betaY;

	// Two KEA-related values
	REncodedElt* betaYgamma;

	LEncodedElt* betaYTatS;		// NIZK-specific	

	// Num bytes needed for this key
	virtual int size();
	virtual void serialize(Archive* arc);
	virtual void deserialize(Archive* arc);

	virtual void initialize(int numPowers, int numPolys);
};

// Holds all of the QAP-specific key elements
class QapSecretKey : public SecretKey {
public:
	QapSecretKey();
	QapSecretKey(bool designatedVerifier);

	FieldElt betaY;

	// Num bytes needed for this key
	virtual int size();
	virtual void serialize(Archive* arc);
	virtual void deserialize(Archive* arc);
};

// Holds all of the QSP-specific key elements
class QspPublicKey : public PublicKey {
	public:
	QspPublicKey(QSP* qsp, int numPowers, int numPolys, bool isNIZK);
	QSP* qsp;

	//~QspPublicKey();
};

#else // !USE_OLD_PROTO

/////////////////////////// New Keys //////////////////////////////////////

class PublicKey {
public:
	PublicKey();
	PublicKey(int numPowers, int numPolys, bool isNIZK, bool designatedVerifier);

	// Powers of s^i
	LEncodedElt* powers;

	// Lots of polys evaluated at s
	LEncodedElt* Vpolys;
	REncodedElt* Wpolys;

	LEncodedElt* alphaVpolys;
	LEncodedElt* alphaWpolys;

	LEncodedElt* betaVWYpolys;

	// KEA values
	REncodedElt* alphaV;
	LEncodedElt* alphaW;

	REncodedElt* gammaR;
	LEncodedElt* betaGammaL;
	REncodedElt* betaGammaR;

	REncodedElt* RtAtS;	

	// Some extra values needed for NIZK	
	LEncodedElt* LVtAtS;
	REncodedElt* RWtAtS;
	LEncodedElt* LalphaVTatS;
	LEncodedElt* LalphaWTatS;	
	LEncodedElt* LbetaVTatS;
	LEncodedElt* LbetaWTatS;
	
	PolyTree* polyTree;			// Precompute polynomial tree, with leaves (x-r), for r in target roots, and internal nodes as products of their children
	FieldElt* denominators;		// Pre-computed Lagrange denominators

	// Num bytes needed for this key
	virtual int size();

	int numPowers;
	int numPolys;
	bool isNIZK;
	bool designatedVerifier;

	virtual void serialize(Archive* arc);
	virtual void deserialize(Archive* arc);

	virtual bool equals(Encoding* encoding, PublicKey* other);

	void cleanup();
	virtual ~PublicKey();

#ifdef __FFLIB
public:
#else
protected:
#endif
	virtual void initialize(int numPowers, int numPolys, bool isNIZK, bool designatedVerifier);
protected:
	bool isEqual(Encoding* encoding, PublicKey* other);
	virtual void printCommon(Encoding* encoding);

private:
	void init();
};

class SecretKey {
public:
	SecretKey();
	SecretKey(bool designatedVerifier);
	~SecretKey();

	bool designatedVerifier;	// If this is false, all other fields are invalid

	FieldElt alphaV;
	FieldElt alphaW;
	FieldElt beta;	
	FieldElt gamma;
	FieldElt rV, rW;

	FieldElt target;

	int sizeOfv;
	FieldElt* v;	// Various v polynomials (v_0, v_in, v_out) evaluated at s
	IndexedElts vVals;

	// Num bytes needed for this key
	virtual int size();
	void cleanup();

	virtual void serialize(Archive* arc);
	virtual void deserialize(Archive* arc);	
};

class Keys {
public:
	Keys(bool designatedVerifier, QAP* qap, int numPowers, int numPolys, bool isNIZK);
	Keys(bool designatedVerifier, QSP* qsp, int numPowers, int numPolys, bool isNIZK);
	~Keys();

	// Raw = 0 means human readable
	void printSizes(size_t raw);

	SecretKey* sk;
	PublicKey* pk;
};


// Holds all of the QAP-specific key elements
class QapPublicKey : public PublicKey {
public:
	QapPublicKey();
	QapPublicKey(QAP* qap, int numPowers, int numPolys, bool isNIZK, bool designatedVerifier);	
	~QapPublicKey();

	QAP* qap;

	// Two sets of polys evaluated at s
	LEncodedElt* Ypolys;
	LEncodedElt* alphaYpolys;

	REncodedElt* alphaY;
	
	LEncodedElt* LYtAtS;			// NIZK-specific	
	LEncodedElt* LalphaYTatS;	// NIZK-specific	
	LEncodedElt* LbetaYTatS;	// NIZK-specific	

	// Num bytes needed for this key
	virtual int size();
	virtual void serialize(Archive* arc);
	virtual void deserialize(Archive* arc);
	virtual void print(Encoding* encoding);
	bool equals(Encoding* encoding, QapPublicKey* other);

	virtual void initialize(int numPowers, int numPolys);
private:
	void QapInit();
};

// Holds all of the QAP-specific key elements
class QapSecretKey : public SecretKey {
public:
	QapSecretKey();
	QapSecretKey(bool designatedVerifier);

	FieldElt rY;
	FieldElt alphaY;

	int numWs;
	FieldElt* alphaWs;	// w polynomials, that correspond to inputs, evaluated at s
	int numYs;
	FieldElt* y;				// y polynomials, that correspond to outputs, evaluated at s

	// Num bytes needed for this key
	virtual int size();
	virtual void serialize(Archive* arc);
	virtual void deserialize(Archive* arc);
};

// Holds all of the QSP-specific key elements
class QspPublicKey : public PublicKey {
	public:
	QspPublicKey(QSP* qsp, int numPowers, int numPolys, bool isNIZK, bool designatedVerifier);
	QSP* qsp;

	//~QspPublicKey();
};

#endif // USE_OLD_PROTO