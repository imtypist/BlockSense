#pragma once

#include "Encoding.h"
#include "Archive.h"

#ifdef USE_OLD_PROTO

/////////////////////////// Old Proof //////////////////////////////////////
class Proof {
public:
        LEncodedElt V;
        REncodedElt W;
        LEncodedElt Y;
        LEncodedElt H;
        LEncodedElt alphaV;
        REncodedElt alphaW;
        LEncodedElt alphaY;
        LEncodedElt alphaH;
        LEncodedElt beta;

        FieldElt* inputs;
        FieldElt* outputs;

        int numInputs;
        int numOutputs;

        Proof();
        Proof(int numIn, int numOut);
        ~Proof();

        void serialize(Archive* arc);
        void deserialize(Archive* arc);
};

class QspProof {
public:
        LEncodedElt V;
        REncodedElt W;
        LEncodedElt H;
        LEncodedElt alphaV;
        REncodedElt alphaW;
        LEncodedElt alphaH;
        LEncodedElt beta;

        bool* inputs;
        bool* outputs;

        int numInputs;
        int numOutputs;

        QspProof() { inputs = NULL; outputs = NULL; }
        QspProof(int numIn, int numOut)  { inputs = new bool[numIn]; outputs = new bool[numOut]; }
        ~QspProof() { delete [] inputs; delete [] outputs; }

        void serialize(Archive* arc);
        void deserialize(Archive* arc);
};
#else // !USE_OLD_PROTO

/////////////////////////// New Proof //////////////////////////////////////

class Proof {
public:
	LEncodedElt V;
	REncodedElt W;
	LEncodedElt Y;
	LEncodedElt H;
	LEncodedElt alphaV;
	LEncodedElt alphaW;
	LEncodedElt alphaY;
	LEncodedElt beta;

	FieldElt* inputs;	 // Convenience pointer
	FieldElt* outputs; // Convenience pointer
	FieldElt* io;	// Actual array with IO values

	int numInputs;
	int numOutputs;

	Proof();
	Proof(int numIn, int numOut);
	~Proof();

	bool equal(Encoding* encoding, Proof* other);
	
	void serialize(Archive* arc);
	void deserialize(Archive* arc);
};

class QspProof {
public:
	LEncodedElt V;
	REncodedElt W;
	LEncodedElt H;
	LEncodedElt alphaV;
	REncodedElt alphaW;
	LEncodedElt alphaH;
	LEncodedElt beta;

	bool* inputs;
	bool* outputs;

	int numInputs;
	int numOutputs;

	QspProof() { inputs = NULL; outputs = NULL; }
	QspProof(int numIn, int numOut)  { inputs = new bool[numIn]; outputs = new bool[numOut]; }
	~QspProof() { delete [] inputs; delete [] outputs; }

	void serialize(Archive* arc);
	void deserialize(Archive* arc);
};

#endif