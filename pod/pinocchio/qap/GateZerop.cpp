#include "GateZerop.h"
#include <iostream>

using namespace std;

GateZerop::GateZerop(Field * field)
{
	field->zero(zero);
	field->one(one);
	this->field = field;
	inputs = new Wire*[1];

	maxInputs = 1;
	assignedInputs = 0;
	
	inputs[0] = NULL;
	
	output = NULL;
}

GateZerop::~GateZerop(){
}

void GateZerop::eval() {
	field->one(one);
	field->zero(zero);
	if(field->equal(inputs[0]->value, zero)) {
		field->one(output->value);
	}
	else
	{
		field->div(one, inputs[0]->value, output->value);
	}
}

#if 0
CachedPolynomial* GateZerop::getCachedPolynomial() {
	return cachedPoly;
}

void GateZerop::cachePolynomial(CachedPolynomial* poly) {
	assert(cachedPoly == NULL);
	cachedPoly = poly;
}

void GateZerop::deleteCachedPolynomial(CachedPolynomial* poly) {
	delete cachedPoly;
}
#endif

bool GateZerop::testAndSetReady() {
	if (!inputs[0]->visited) {
		return false;
	}
	return true;
}

void GateZerop::assignPolyCoeff(QAP *qap) {
	qap->V[qap->polyIndex(this)].coeff = &this->output->value;
	qap->W[qap->polyIndex(this)].coeff = &this->output->value;
	qap->Y[qap->polyIndex(this)].coeff = &this->output->value;
}

void GateZerop::generatePolys(QAP * qap, bool cachedBuild)
{
	Modifier mod;	// Create a NOP modifier to start with
	field->zero(mod.add);
	field->one(mod.mul);

	// Adds the "r1" root (which is the root for this wire) to the V root of the input
	recurseTopological(qap, inputs[0],qap->V,mod,cachedBuild);
	
	// add the v0 constraint
	FieldElt one;
	field->one(one);
	qap->V[0].addNonZeroRoot(next->rootId, one, this->field);

	qap->W[qap->polyIndex(this)].addNonZeroRoot(this->rootId, one, this->field);
}

class GateZeropY;

class GateZeropY : public GateZerop {
public:
	GateZeropY(Field * field) : GateZerop(field) {
	}

	virtual int polyCount(){return 1;}
	
	void eval() {
		assert(inputs[0] && output);
		this->field->zero(zero);
		this->field->zero(output->value);

		if(this->field->equal(inputs[0]->value, zero))
			this->field->zero(this->output->value);
		else
			this->field->one(this->output->value);
	}

	bool testAndSetReady() {
		if (!inputs[0]->visited) {
			return false;
		}
		output->visited = true;
		return true;
	}

	virtual void generatePolys(QAP* qap, bool cachedBuild){
		Modifier mod;	// Create a NOP modifier to start with
		field->zero(mod.add);
		field->one(mod.mul);

		// Adds the "r2" root (which is the root for this gate) to the W root of the input
		recurseTopological(qap, inputs[0],qap->W,mod,cachedBuild);

		FieldElt negOne, one;
		field->one(one);
		field->sub(zero, one, negOne);
		qap->V[qap->polyIndex(this)].addNonZeroRoot(this->rootId, negOne, this->field);
		qap->Y[qap->polyIndex(this)].addNonZeroRoot(prev->rootId, one, this->field);
	}

	virtual void assignPolyCoeff(QAP* qap) {
		qap->V[qap->polyIndex(this)].coeff = &this->output->value;
		qap->W[qap->polyIndex(this)].coeff = &this->output->value;
		qap->Y[qap->polyIndex(this)].coeff = &this->output->value;
	}

	virtual list<GateZerop*> getPolyGeneratorList(){
		list<GateZerop*> ret;
		ret.push_front(this);
		return ret;
	}
private:
	int r1, r2;
};

list<GateZerop*> GateZerop::getPolyGeneratorList() {
	list<GateZerop*> ret;
	ret.push_front(this);

	this->next = new GateZeropY(this->field);
	next->prev = this;

	list<GateZerop*> rest = next->getPolyGeneratorList();
	ret.insert(ret.end(), rest.begin(), rest.end());
	return ret;
}