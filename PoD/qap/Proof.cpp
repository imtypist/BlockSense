#define WIN32_LEAN_AND_MEAN  // Prevent conflicts in Window.h and WinSock.h
#include "Proof.h"
#include <string.h>


#ifdef USE_OLD_PROTO
/////////////////////////// Old Proof //////////////////////////////////////

Proof::Proof() {
        inputs = NULL;
        outputs = NULL;
}

Proof::Proof(int numIn, int numOut)  {
        numInputs = numIn;
        numOutputs = numOut;
        inputs = new FieldElt[numIn];
        outputs = new FieldElt[numOut];
}

Proof::~Proof() {
        delete [] inputs;
        delete [] outputs;
}

void Proof::serialize(Archive* arc) {
        arc->write(numInputs);
        arc->write(numOutputs);

        arc->write(V);
        arc->write(W);
        arc->write(Y);
        arc->write(H);
        arc->write(alphaV);
        arc->write(alphaW);
        arc->write(alphaY);
        arc->write(alphaH);
        arc->write(beta);

        arc->write(inputs, numInputs);
        arc->write(outputs, numOutputs);
}

void Proof::deserialize(Archive* arc) {
        arc->read(numInputs);
        arc->read(numOutputs);

        inputs = new FieldElt[numInputs];
        outputs = new FieldElt[numOutputs];

        arc->read(V);
        arc->read(W);
        arc->read(Y);
        arc->read(H);
        arc->read(alphaV);
        arc->read(alphaW);
        arc->read(alphaY);
        arc->read(alphaH);
        arc->read(beta);

        arc->read(inputs, numInputs);
        arc->read(outputs, numOutputs);
}

#else // !USE_OLD_PROTO

/////////////////////////// New Proof //////////////////////////////////////

Proof::Proof() { 
	inputs = NULL; 
	outputs = NULL; 
}

Proof::Proof(int numIn, int numOut)  { 
	numInputs = numIn; 
	numOutputs = numOut; 
	io = new FieldElt[numIn + numOut];
	inputs = io; 
	outputs = io + numIn; 
}

Proof::~Proof() { 
	delete [] io; 
}
	
void Proof::serialize(Archive* arc) {
	arc->write(V);
	arc->write(W);
	arc->write(Y);
	arc->write(H);
	arc->write(alphaV);
	arc->write(alphaW);
	arc->write(alphaY);
	arc->write(beta);
			 
	// Proof only consists of the crypto elements
	// Let someone else handle the I/O
	//arc->write(numInputs);
	//arc->write(numOutputs);
	//arc->write(io, numInputs + numOutputs);
}

void Proof::deserialize(Archive* arc) {
	arc->read(V);
	arc->read(W);
	arc->read(Y);
	arc->read(H);
	arc->read(alphaV);
	arc->read(alphaW);
	arc->read(alphaY);
	arc->read(beta);

	// Proof only consists of the crypto elements
	// Let someone else handle the I/O
	//arc->read(numInputs);
	//arc->read(numOutputs);
	//inputs = new FieldElt[numInputs]; 
	//outputs = new FieldElt[numOutputs];
	//arc->read(io, numInputs + numOutputs);
}

#define CHECK(entry, str) \
	if (!encoding->equals(entry, other->entry)) { printf("%s differs\n", str); equal = false; }

bool Proof::equal(Encoding* encoding, Proof* other) {
	bool equal = true;
	CHECK(V, "V");
	CHECK(W, "W");
	CHECK(Y, "Y");
	CHECK(H, "H");
	CHECK(alphaV, "alphaV");
	CHECK(alphaW, "alphaW");
	CHECK(alphaY, "alphaY");
	CHECK(beta, "beta");

	for (int i = 0; i < numInputs + numOutputs; i++) {
		if (!encoding->getSrcField()->equal(io[i], other->io[i])) { 
			printf("input %d differs\n", i);
			equal = false;
		}		
	}

	return equal;
}

#endif // USE_OLD_PROTO