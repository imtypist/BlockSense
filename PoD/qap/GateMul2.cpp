#include <assert.h>
#include "GateMul2.h"
#include "Field.h"

GateMul2::GateMul2() {
	rootId = -1;
	polyId = -2;
}

GateMul2::GateMul2(Field* field, Wire* in1, Wire* in2, Wire* out) : Gate(field) {
	inputs = new Wire*[2];
	maxInputs = 2;
	assignedInputs = 0;

	inputs[0] = in1;
	inputs[1] = in2;
	output = out;

	if (in1) { in1->output = this; assignedInputs++; }
	if (in2) { in2->output = this; assignedInputs++; }
	if (out) { out->input = this; }
}

GateMul2::~GateMul2() {
	delete[] inputs;
}

inline void GateMul2::eval() {
	assert(inputs[0] && inputs[1] && output);
	field->mul(inputs[0]->value, inputs[1]->value, output->value);	
}

// How many polys does this gate need.  Most need 0 or 1.
int GateMul2::polyCount() {
	// Normal mul gates get 1 root and 1 poly
	return 1;
}

void GateMul2::generatePolys(QAP* qap, bool cachedBuild) {
	Modifier mod;	// Create a NOP modifier to start with
	field->zero(mod.add);
	field->one(mod.mul);

	// Add the root corresponding to this Mult2 gate to the V polynomials
	// corresponding to the gates that contribute to the left input of this gate
	recurseTopological(qap, inputs[0], qap->V, mod, cachedBuild);

	// Same, but take the contributors to the right input and add to the W polys
	recurseTopological(qap, inputs[1], qap->W, mod, cachedBuild);

	// Finally, add an output constraint as well
	FieldElt one;
	field->one(one);
	qap->Y[qap->polyIndex(this)].addNonZeroRoot(this->rootId, one, field);
}

// Assign the value of this gate's output as the coefficient for all of the polynomials
void GateMul2::assignPolyCoeff(QAP* qap) {
	qap->V[qap->polyIndex(this)].coeff = &this->output->value;
	qap->W[qap->polyIndex(this)].coeff = &this->output->value;
	qap->Y[qap->polyIndex(this)].coeff = &this->output->value;
}

// Check off the current branch, then check if we've completed this poly's entire subtree.  
// If so, update it's parent and check the parent.
void completeBranch(CachedPolynomial* poly, Field* field) {
	poly->completionCtr--;
	if (poly->completionCtr == 0) {	// We're done with this child's entire subtree
		if (poly->parent != NULL) {
			// Add this poly's effects to its parent
			//poly->parent->me.addPoly(poly->me, field);
			poly->parent->me.addModifiedPoly(poly->me, poly->parent->myMod.mul, field);			
			completeBranch(poly->parent, field);
		}
	}
}

// Implemented iteratively, so we don't pop the stack while recursing
// TODO: Don't really need the divisions in the cached poly case. 
// Just need to use wireMod.my, as long as we update that mod for each gate we traverse, if we decide not to cache at every gate
void GateMul2::recurseTopological(QAP* qap, Wire* wire, SparsePolynomial* polys, Modifier mod, bool cachedBuild) {	
	WireModList wireModStack;

	WireMod* newMod = new WireMod;
	newMod->cachedPoly = NULL;
	newMod->mod = mod;
	newMod->wire = wire;
	wireModStack.push_front(newMod);

	while (!wireModStack.empty()) {
		WireMod* wireMod = wireModStack.front();
		wireModStack.pop_front();

		if (wireMod->wire->input == NULL) {	// This is an input to the circuit
			polys[qap->polyIndex(wireMod->wire)].addNonZeroRoot(this->rootId, wireMod->mod.mul, field);
			if (wireMod->cachedPoly != NULL) {
				// Memoize this input in the poly summarizing some "lower" gate
				FieldElt val;  // TODO: Should also subtract off the additive portion
				field->div(wireMod->mod.mul, wireMod->cachedPoly->initMod.mul, val);		// Divide out init value, so only capture effects "above" the cached gate
				wireMod->cachedPoly->me.addNonZeroRoot((unsigned long)qap->polyIndex(wireMod->wire), val, field);					
				completeBranch(wireMod->cachedPoly, field); // Are we done with this subtree?
			}
		} else {
			Gate* gate = wireMod->wire->input;

			if (gate->isMult()) {
				// Base case
				polys[qap->polyIndex((GateMul2*)gate)].addNonZeroRoot(this->rootId, wireMod->mod.mul, field);				
				if (wireMod->cachedPoly != NULL) {
					// Memoize this mulGate in the poly summarizing some "lower" gate
					FieldElt val;	// TODO: Should also subtract off the additive portion
					field->div(wireMod->mod.mul, wireMod->cachedPoly->initMod.mul, val);		// Divide out init value, so only capture effects "above" the cached gate
					wireMod->cachedPoly->me.addNonZeroRoot((unsigned long)qap->polyIndex((GateMul2*)gate), val, field);
					completeBranch(wireMod->cachedPoly, field); // Are we done with this subtree?
				}
			} else {
				CachedPolynomial* gateSummary = gate->getCachedPolynomial();
				
				if (gateSummary != NULL) {  // We've been here already.  
					assert(gateSummary->completionCtr == 0);		// If we reach a gate with a poly, it should be complete by now due to DFS
					// Apply the gate that started the recursion (this), and the appropriate modifier, 
					// to all of the polys reachable from the gate we've reached (summarized in gateSummary)
					for (gateSummary->me.iterStart(); !gateSummary->me.iterEnd(); gateSummary->me.iterNext()) {						
						FieldElt val;   // TODO: Should also handle addition
						field->mul(*gateSummary->me.curRootVal(), wireMod->mod.mul, val);
						polys[gateSummary->me.curRootID()].addNonZeroRoot(this->rootId, val, field); 
					}

					// Update the poly we're building (if any) to reflect the modifier we've built up on our journey to this gate + the cached poly here
					if (wireMod->cachedPoly != NULL) {
						FieldElt val;	// TODO: Should also subtract off the additive portion
						field->div(wireMod->mod.mul, wireMod->cachedPoly->initMod.mul, val);		// Divide out init value, so only capture effects "above" the cached gate
						wireMod->cachedPoly->me.addModifiedPoly(gateSummary->me, val, field);
						completeBranch(wireMod->cachedPoly, field);  // Are we done with this subtree?
					}
				} else {
					if (cachedBuild) {
						// This gate doesn't have a polynomial to summarize its effects, so create a fresh poly
						gateSummary = new CachedPolynomial();  
						gateSummary->parent = wireMod->cachedPoly;
						gateSummary->completionCtr = gate->numInputs();		// Poly will be complete when we've handled all its children
						memcpy(&gateSummary->initMod, &wireMod->mod, sizeof(Modifier));
						field->one(gateSummary->myMod.mul);
						field->zero(gateSummary->myMod.add);
						gate->updateModifier(&gateSummary->myMod);
						gate->cachePolynomial(gateSummary);
					}

					// Recurse on each of the input wires, using an appropriately updated modifier	
					for (int i = 0; i < gate->numInputs(); i++) {
						WireMod* newMod = new WireMod;
						newMod->wire = gate->inputs[i];
						memcpy(&newMod->mod, &wireMod->mod, sizeof(Modifier));
						gate->updateModifier(&newMod->mod);
						newMod->cachedPoly = gateSummary;
						wireModStack.push_front(newMod);		// Push front is important!  Ensures we do DFS, and hence complete a gate's poly before hitting it via another path
					}
				}
			}
		}		
		delete wireMod;
	}
}
