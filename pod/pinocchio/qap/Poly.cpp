#include "Poly.h"
#include "Util.h"
#include <stdio.h>
#include <assert.h>

// Static class members
modfft_info_t Poly::fftinfo;
bigctx_t Poly::BignumCtx;
bigctx_t* Poly::pbigctx;

void Poly::init(Field* field, int degree) {
	this->field = field;
	this->degree = degree;
	this->size = degree + 1;
	this->coefficients = new FieldElt[this->size];
}

Poly::Poly(Field* field, int degree) {
	init(field, degree);
}

Poly::Poly(Field* field, int degree, int* coefficients) {
	init(field, degree);
	
	for (int i = 0; i < this->size; i++) {
		field->set(this->coefficients[i], coefficients[i]);
	}
}

Poly::~Poly() {
	//delete [] coefficients;
}

void Poly::setDegree(int d) {
	reserveSpace(d);
	this->degree = d;
}

// Set the coefficients to the array provided
void Poly::setCoefficients(FieldElt* coeff, int degree) {
	if (coeff != coefficients) {
		delete [] coefficients;
		coefficients = coeff;
		size = degree + 1;		
	}

	this->degree = degree;
}

// Get a pointer to an array large enough to hold coeff for a degree d poly
FieldElt* Poly::getCoefficientArray(int degree) {
	if (degree + 1 > size) {
		return new FieldElt[degree+1];
	} else {
		return coefficients;
	}
}


// Reserve enough space to ensure we can represent a poly of degree d
void Poly::reserveSpace(int d) {
	if (d + 1 > size) {	// Need more room!
		delete [] coefficients;
		coefficients = new FieldElt[d + 1];
		size = d + 1;
	} 
}

void Poly::print() {
	printf("Degree %d, (", degree);
	for (int i = 0; i < degree + 1; i++) {
		field->print(coefficients[i]);
		printf("\t");
	}
	printf(")");
}
	
bool Poly::equals(Poly& p) {
	int minLen = min(degree, p.getDegree()) + 1;

	for (int i = 0; i < minLen; i++) {
		if (!field->equal(coefficients[i], p.coefficients[i])) {
			return false;
		}
	}

	// Check that the rest of the coefficients are 0 for the poly with larger degree
	FieldElt zero;
	field->zero(zero);
	for (int i = minLen; i < max(degree, p.getDegree()) + 1; i++) {
		if (degree < p.getDegree()) {
			if (!field->equal(p.coefficients[i], zero)) {
				return false;
			}
		} else {
			if (!field->equal(coefficients[i], zero)) {
				return false;
			}
		}
	}

	return true;	
}


void Poly::firstDerivative(Poly& poly) {
	firstDerivative(poly, poly);
}

void Poly::firstDerivative(Poly& poly, Poly& result) {
	FieldElt* resultCoeff = result.getCoefficientArray(poly.getDegree()-1);

	for (int d = 1; d < poly.getDegree() + 1; d++) {
		// result.coefficients[d-1] = poly.coefficients[d] * d
		FieldElt degree;
		poly.field->set(degree, d);
		poly.field->mul(poly.coefficients[d], degree, resultCoeff[d-1]);
	}
	//zero(resultCoeff[poly.getDegree()]);
	result.setCoefficients(resultCoeff, poly.getDegree()-1);
}

// Implements high-school-style multiplication as a sanity check on the FFT version
void Poly::mulSlow(Poly& a, Poly& b, Poly& result) {
	int resultDegree = a.getDegree() + b.getDegree();
	FieldElt* resultCoeff = result.getCoefficientArray(resultDegree);

	a.field->zero(resultCoeff, resultDegree + 1);

	FieldElt temp;
	for (int da = 0; da < a.getDegree() + 1; da++) {
		for (int db = 0; db < b.getDegree() + 1; db++) {
			a.field->mul(a.coefficients[da], b.coefficients[db], temp);
			a.field->add(resultCoeff[da+db], temp, resultCoeff[da+db]);
		}
	}

	result.setCoefficients(resultCoeff, resultDegree);
}

// Multiply a polynomial p by a constant c
void Poly::constMul(Poly& p, FieldElt& c, Poly& result) {
	FieldElt* resultCoeff = result.getCoefficientArray(p.getDegree());

	for (int d = 0; d < p.getDegree() + 1; d++) {
		p.field->mul(p.coefficients[d], c, resultCoeff[d]);
	}

	result.setCoefficients(resultCoeff, p.getDegree());
}

// Makes a polynomial monic.  Sets c to be coeff of x^maxdegree
// TODO: Optimize if p is already monic
void Poly::makeMonic(Poly& p, Poly& result, FieldElt &leadingCoeff) {
	FieldElt inverse, uno, nada;
	p.field->one(uno);
	p.field->zero(nada);

	// Trim any leading 0s
	int resultDegree = p.getDegree();
	while (p.field->equal(p.coefficients[resultDegree], nada) && resultDegree > 0) {
		resultDegree--;
	}

	result.setDegree(resultDegree);

	for (int d = 0; d < resultDegree + 1; d++) {
		p.field->copy(p.coefficients[d], result.coefficients[d]);
	}
	
	
	if (p.field->equal(result.coefficients[result.getDegree()], uno)) {
		// Already monic
		p.field->one(leadingCoeff);
	} else {
		// Monicize by dividing all coefficients by the leading coefficient
		p.field->copy(result.coefficients[result.getDegree()], leadingCoeff);
		p.field->div(uno, leadingCoeff, inverse);
		
		constMul(result, inverse, result);
	}
}

bool Poly::isZero(Poly& p) {
	FieldElt nada;
	p.field->zero(nada);

	for (int d = 0; d < p.getDegree() + 1; d++) {
		if (!p.field->equal(p.coefficients[d], nada)) {
			return false;
		}
	}
	return true;
}

// Computes: result <- a * b
void Poly::mul(Poly& a, Poly& b, Poly& result) {
	//mulSlow(a, b, result);

	bool aMonic = a.field->equal(a.coefficients[a.getDegree()], *(FieldElt*)a.field->msfield->one);
	bool bMonic = a.field->equal(b.coefficients[b.getDegree()], *(FieldElt*)a.field->msfield->one);
	bool monic = aMonic && bMonic;

	// Special case the easiest cases
	if (a.getDegree() == 0 && b.getDegree() == 0) {
		result.setDegree(0);
		a.field->mul(a.coefficients[0], b.coefficients[0], result.coefficients[0]);
	} else if (isZero(a) || isZero(b)) {
		result.setDegree(a.getDegree() + b.getDegree());
		zero(result, result.getDegree());
	} else {
		// Make both inputs monic
		Poly monicA(a.field), monicB(a.field);
		FieldElt monicCorrection, aCorrection, bCorrection;

		makeMonic(a, monicA, aCorrection);			
		makeMonic(b, monicB, bCorrection);
		a.field->mul(aCorrection, bCorrection, monicCorrection);

		result.setDegree(monicA.getDegree() + monicB.getDegree());

		// Special case the small, easy values
		if (monicA.getDegree() == 1 && monicB.getDegree() == 1) {  // (x+a)(x+b)
			a.field->one(result.coefficients[2]);                                                   // x^2
			a.field->add(monicA.coefficients[0], monicB.coefficients[0], result.coefficients[1]);	// (a+b)x^1
			a.field->mul(monicA.coefficients[0], monicB.coefficients[0], result.coefficients[0]);	// (a*b)x^0		
		} else if (monicA.getDegree() == 2 && monicB.getDegree() == 1) {  // (x^2+ax+b)(x+c) = x^3 + ax^2 + cx^2 + bx + acx + bc
			a.field->one(result.coefficients[3]);	                                            	// x^3
			a.field->add(monicA.coefficients[1], monicB.coefficients[0], result.coefficients[2]);	// (a+c)x^2
			a.field->mul(monicA.coefficients[1], monicB.coefficients[0], result.coefficients[1]);
			a.field->add(result.coefficients[1], monicA.coefficients[0], result.coefficients[1]);	// (ac+b)x^1
			a.field->mul(monicA.coefficients[0], monicB.coefficients[0], result.coefficients[0]);	// (b*c)x^0		
		} else if (monicA.getDegree() == 2 && monicB.getDegree() == 2) { // (x^2+ax+b)(x^2+cx+d) = x^4 + ax^3 + bx^2 + cx^3 + acx^2 + bcx + dx^2 + adx + bd
			FieldElt temp;
			a.field->one(result.coefficients[4]);	                                            	// x^4
			a.field->add(monicA.coefficients[1], monicB.coefficients[1], result.coefficients[3]);	// (a+c)x^3
			a.field->mul(monicA.coefficients[1], monicB.coefficients[1], result.coefficients[2]);
			a.field->add(result.coefficients[2], monicA.coefficients[0], result.coefficients[2]);
			a.field->add(result.coefficients[2], monicB.coefficients[0], result.coefficients[2]);	// (ac + b + d)x^2
			a.field->mul(monicA.coefficients[0], monicB.coefficients[1], temp);
			a.field->mul(monicA.coefficients[1], monicB.coefficients[0], result.coefficients[1]);
			a.field->add(result.coefficients[1], temp, result.coefficients[1]);	// (bc+ad)x^1
			a.field->mul(monicA.coefficients[0], monicB.coefficients[0], result.coefficients[0]); // (bd)x^0
		} else {	// Use an FFT		
			int lenA = monicA.getDegree() + 1;
			int lenB = monicB.getDegree() + 1;

			const int lg_convol_lng = (int)significant_bit_count((digit_t)(lenA + lenB - 1));
			int convol_lng = (int)1 << lg_convol_lng;
			convol_lng *= (int)fftinfo.nprime;

			// TODO: Cache this memory to avoid allocating every time
			FieldElt* scratch1 = new FieldElt[convol_lng];
			FieldElt* scratch2 = new FieldElt[convol_lng];

			lenA--;
			lenB--;

			/*
			// modpoly_monic_product expects a not-necessarily monic poly of length X
			// If the polys are already monic, we can reduce the poly work by 1
			if (aMonic) { lenA--; }
			if (bMonic) { lenB--; }
			*/

			if (lenA + lenB - 1 > result.getDegree()) {
				assert(0);
				// We need more space to hold the intermediate result
				// TODO: Find a more elegant solution
				FieldElt* temp = new FieldElt[lenA + lenB];
				modpoly_monic_product((digit_t*)monicA.coefficients, lenA, (digit_t*)monicB.coefficients, lenB, (digit_t*)temp, scratch1[0], scratch2[0], &fftinfo, PBIGCTX_PASS);
				// Take the result mod x^(result.degree+1)
				for (int i = 0; i < result.getDegree() + 1; i++) {
					a.field->copy(temp[i], result.coefficients[i]);
				}	
				delete [] temp;
			} else {
				modpoly_monic_product((digit_t*)monicA.coefficients, lenA, (digit_t*)monicB.coefficients, lenB, (digit_t*)result.coefficients, scratch1[0], scratch2[0], &fftinfo, PBIGCTX_PASS);
				a.field->one(result.coefficients[result.getDegree()]);	// modpoly_monic_product doesn't bother with the x.getDegree() coefficient, since it assumes it's monic
			}		

			delete [] scratch1;
			delete [] scratch2;
		}

		if (!monic) { // Need to reverse the corrections we made earlier to convert a and b to monic polys
			constMul(result, monicCorrection, result);
		}
	}

}


// result <- a + b
void Poly::add(Poly& a, Poly& b, Poly& result) {
	int resultDegree = max(a.getDegree(), b.getDegree());
	FieldElt* resultCoeff = result.getCoefficientArray(resultDegree);

	for (int i = 0; i < min(a.getDegree(), b.getDegree()) + 1; i++) {
		a.field->add(a.coefficients[i], b.coefficients[i], resultCoeff[i]);
	}

	if (a.getDegree() < b.getDegree()) {
		for (int i = a.getDegree()+1; i < b.getDegree() + 1; i++) {
			a.field->copy(b.coefficients[i], resultCoeff[i]);
		}
	} else if (b.getDegree() < a.getDegree() + 1) {
		for (int i = b.getDegree()+1; i < a.getDegree()+1; i++) {
			a.field->copy(a.coefficients[i], resultCoeff[i]);
		}
	}

	result.setCoefficients(resultCoeff, resultDegree);
}

// result <- a - b
void Poly::sub(Poly& a, Poly& b, Poly& result) {
	int resultDegree = max(a.getDegree(), b.getDegree());
	FieldElt* resultCoeff = result.getCoefficientArray(resultDegree);

	for (int i = 0; i < min(a.getDegree(), b.getDegree()) + 1; i++) {
		a.field->sub(a.coefficients[i], b.coefficients[i], resultCoeff[i]);
	}

	if (a.getDegree() < b.getDegree()) {
		FieldElt nada;
		a.field->zero(nada);

		for (int i = a.getDegree()+1; i < b.getDegree() + 1; i++) {
			a.field->sub(nada, b.coefficients[i], resultCoeff[i]);
		}
	} else if (b.getDegree() < a.getDegree()) {
		for (int i = b.getDegree()+1; i < a.getDegree() + 1; i++) {
			a.field->copy(a.coefficients[i], resultCoeff[i]);
		}
	}

	result.setCoefficients(resultCoeff, resultDegree);
}

// Reverse into a fresh polynomial
void Poly::reverse(Poly& f, Poly& fRev) { 
	fRev.setDegree(f.getDegree());
	
	for (int i = 0; i < f.getDegree()+1; i++) {
		f.field->copy(f.coefficients[i], fRev.coefficients[(f.getDegree()+1) - i - 1]);
	}
}

// Reverse in place
void Poly::reverse(Poly& f) { 
	FieldElt tmp;
	for (int i = 0; i < (f.getDegree()+1) / 2; i++) {
		f.field->copy(f.coefficients[i], tmp);
		f.field->copy(f.coefficients[(f.getDegree()+1) - i - 1], f.coefficients[i]);
		f.field->copy(tmp, f.coefficients[(f.getDegree()+1) - i - 1]);
	}
}

// Compute f mod x^degree
void Poly::reduce(Poly& f, int degree) {
	assert(degree > 0);

	if (f.getDegree() >= degree) {
		f.setDegree(degree - 1);
	}
	// Else, no effect
}

// Create a polynomial that has all 0 coefficients
void Poly::zero(Poly& f, int degree) {
	f.setDegree(degree);	
	f.field->zero(f.coefficients, degree+1);
}

// Compute: result <- f / x^degree
void Poly::div(Poly& f, int degree, Poly& result) {
	if (f.getDegree() - degree < 0) {	// Should only happen if we're dividing the 0 poly
		FieldElt nada;
		f.field->zero(nada);
		for (int i = 0; i < f.getDegree(); i++) {
			assert(f.field->equal(f.coefficients[i], nada));
		}
		result.setDegree(0);
		f.field->zero(result.coefficients[0]);
	} else {
		result.setDegree(f.getDegree() - degree);	

		for (int i = 0; i < result.getDegree()+1; i++) {
			f.field->copy(f.coefficients[i+degree], result.coefficients[i]);
		}
	}
}

// Compute result <- f^-1 mod x.getDegree()
// Algorithm based on http://people.csail.mit.edu/madhu/ST12/scribe/lect06.pdf
void Poly::invert(Poly& f, int degree, Poly& result) {
	FieldElt uno; f.field->one(uno);
	assert(f.field->equal(f.coefficients[0], uno));

	if (degree == 0) {
		assert(0); // TODO
	} else if (degree == 1) {	// Because we know the constant coeff is 1, Inv(f) mod x = 1
		result.setDegree(0);
		f.field->one(result.coefficients[0]);
	} else {
		int splitDegree;

		if (degree % 2 == 1) {
			splitDegree = degree - 1;
		} else {
			splitDegree = degree / 2;
		}

		Poly inverse0(f.field), inverse0sqr(f.field), inverse1(f.field);
		invert(f, splitDegree, inverse0);	// inverse0 * f = 1 mod x^{d/2}
		mul(inverse0, inverse0, inverse0sqr);

		// Let f = f0 + f1 * x^{d/2}
		Poly f0(f.field), f1(f.field);
		f0.setDegree(splitDegree - 1);
		f1.setDegree(degree - splitDegree);		
		for (int i = 0; i < f0.getDegree() + 1; i++) {
			if (i < f.getDegree() + 1) {
				f.field->copy(f.coefficients[i], f0.coefficients[i]);
			} else { // f is 0 here
				f.field->zero(f0.coefficients[i]);
			}
		}
		for (int i = 0; i < f1.getDegree() + 1; i++) {
			int fIndex = i + splitDegree;
			if (fIndex < f.getDegree() + 1) {
				f.field->copy(f.coefficients[fIndex], f1.coefficients[i]);
			} else { // f is 0 here
				f.field->zero(f1.coefficients[i]);
			}			
		}		

		// inverse1 = -(inverse0^2 * f1 + inverse0 * b), where b = x^{-d/2} (inverse0 * f0 - 1)
		// => inverse1 = -(inverse0^2 * f1 + x^{-d/2} (inverse0^2 * f0 - inverse0))		
		Poly temp(f.field), nada(f.field);
		zero(nada, splitDegree);
		mul(inverse0sqr, f0, temp);		// temp = inverse0^2 * f0
		sub(temp, inverse0, temp);		// temp = (inverse0^2 * f0 - inverse0)
		div(temp, splitDegree, temp);	// temp = x^{-d/2} (inverse0^2 * f0 - inverse0)
		mul(inverse0sqr, f1, inverse1);	// inverse1 = inverse0^2 * f1
		add(inverse1, temp, inverse1);	// inverse1 = (inverse0^2 * f1 + x^{-d/2} (inverse0^2 * f0 - inverse0))
		zero(nada, inverse1.getDegree());
		sub(nada, inverse1, inverse1);	// inverse1 *= -1
		
		// Finally, result = inverse0 + x^{splitDegree} inverse1
		result.setDegree(min(degree - 1, inverse1.getDegree() + splitDegree));
		for (int i = 0; i < splitDegree; i++) {
			int inverse0index = i;
			if (inverse0index < inverse0.getDegree() + 1) {
				f.field->copy(inverse0.coefficients[i], result.coefficients[i]);
			} else {
				f.field->zero(result.coefficients[i]);
			}
		}
		for (int i = splitDegree; i < result.getDegree()+1; i++) {
			int inverse1index = i - splitDegree;
			if (inverse1index < inverse1.getDegree() + 1) {
				f.field->copy(inverse1.coefficients[inverse1index], result.coefficients[i]);
			} else {
				f.field->zero(result.coefficients[i]);
			}
		}
	}
}

// Computes r, q such that f = q*g + r
// Algorithm based on http://people.csail.mit.edu/madhu/ST12/scribe/lect06.pdf
// Note: Rev(q) = Rev(g)^-1 * Rev(f) mod (x^(deg(f)+deg(g)))
// Requires that g is monic
void Poly::mod(Poly& f, Poly& g, Poly& q, Poly& r) {
	Poly revF(f.field), revG(f.field);
	FieldElt uno; f.field->one(uno);
	assert(f.field->equal(g.coefficients[g.getDegree()], uno));
	assert(g.getDegree() >= 1);	// Can't take mods of a constant

	if (f.getDegree() < g.getDegree()) {  // No work needs to be done
		FieldElt* rCoeff = r.getCoefficientArray(f.getDegree());		
		for (int i = 0; i < f.getDegree() + 1; i++) {
			f.field->copy(f.coefficients[i], rCoeff[i]);
		}
		zero(q, g.getDegree());
		r.setCoefficients(rCoeff, f.getDegree());	
	} else {
		reverse(f, revF);
		reverse(g, revG);

		invert(revG, f.getDegree()-g.getDegree()+1, revG);	
		mul(revF, revG, q);
		reduce(q, f.getDegree()-g.getDegree()+1);

		reverse(q);

		Poly temp(f.field);
		mul(g, q, temp);	
		sub(f, temp, r);	// r <- f - q*g
		reduce(r, g.getDegree());
	}
}

void Poly::multiEval(Poly& f, FieldElt* x, int len, FieldElt* result) {
	PolyTree* tree = new PolyTree(f.field, x, len);
	multiEval(f, x, len, result, tree, 0, 0, tree->height - 1);
	delete tree;
}




// Evaluate f at x, which contains len points, and place the result in the result array, starting at index resultIndex.
// Uses a precomputed tree of polynomials, which we traverse top down
void Poly::multiEval(Poly& f, FieldElt* x, int len, FieldElt* result, PolyTree* tree, int resultIndex, int polyIndex, int depth) {
	if (len == 0) {
		assert(0);
	} else if (depth == 0) {
		f.field->copy(f.coefficients[0], result[resultIndex]);
	} else {
		int newLen = (int) pow(2.0, (int)(log2(len)-1));

		if (len < pow(2.0, depth) + pow(2.0, depth-1)) {	// Divide in two
			Poly r0(f.field), r1(f.field), q(f.field);
			mod(f, *tree->polys[depth-1][polyIndex], q, r0);
			mod(f, *tree->polys[depth-1][polyIndex+1], q, r1);

			multiEval(r0, x, newLen, result, tree, resultIndex, 2*polyIndex, depth-1);
			multiEval(r1, &x[newLen], len - newLen, result, tree, resultIndex+newLen, 2*(polyIndex+1), depth-1);
		} else {	// Divide in three
			Poly r0(f.field), r1(f.field), r2(f.field), q(f.field);
			mod(f, *tree->polys[depth-1][polyIndex], q, r0);
			mod(f, *tree->polys[depth-1][polyIndex+1], q, r1);
			mod(f, *tree->polys[depth-1][polyIndex+2], q, r2);

			multiEval(r0, x, newLen, result, tree, resultIndex, 2*polyIndex, depth-1);
			multiEval(r1, &x[newLen], newLen, result, tree, resultIndex+newLen, 2*(polyIndex+1), depth-1);
			multiEval(r2, &x[2*newLen], len - 2*newLen, result, tree, resultIndex+2*newLen, 2*(polyIndex+2), depth-1);
		}

	}
}

// Interpolate the len points (x_i,y'_i), using the auxilliary info in the polynomial tree, placing the result in Poly& result
// Interpolate with respect to the polynomials at level depth in the tree, and index polyIndex within that particular level
void Poly::interpolateRecursive(Field* field, FieldElt* y, PolyTree* tree, Poly& result, int len, int depth, int polyIndex) {
	if (len == 0) {
		assert(0);
	} else if (depth == 0) {
		result.setDegree(0);
		field->copy(y[0], result.coefficients[0]);
	} else {
		int newLen = (int) pow(2.0, (int)(log2(len)-1));

		if (len < pow(2.0, depth) + pow(2.0, depth-1)) {	// Divide in two
			Poly r0(field), r1(field);
		
			interpolateRecursive(field, y, tree, r0, newLen, depth-1, 2*polyIndex);
			interpolateRecursive(field, (FieldElt*)y[newLen], tree, r1, len - newLen, depth-1, 2*(polyIndex+1));

			Poly result0(field), result1(field);
			mul(r0, *tree->polys[depth-1][polyIndex+1], result0);
			mul(r1, *tree->polys[depth-1][polyIndex], result1);
			add(result0, result1, result);
		} else {	// Divide in three
			Poly r0(field), r1(field), r2(field);
		
			interpolateRecursive(field, y, tree, r0, newLen, depth-1, 2*polyIndex);
			interpolateRecursive(field, (FieldElt*)y[newLen], tree, r1, newLen, depth-1, 2*(polyIndex+1));
			interpolateRecursive(field, (FieldElt*)y[2*newLen], tree, r2, len - 2*newLen, depth-1, 2*(polyIndex+2));

			Poly result0(field), result1(field), result2(field);
			//mul(r0, *tree->polys[depth-1][levelindex+1], result0);
			//mul(r1, *tree->polys[depth-1][levelindex], result1);
			//mul(r2, *tree->polys[depth-1][levelindex+2], result2);
			//add(result0, result1, result);
			//add(result, result2, result);

			// Proposed fix.  TODO: Should precompute these extra multiplications
			mul(*tree->polys[depth-1][polyIndex+1], *tree->polys[depth-1][polyIndex+2], result0);
			mul(r0, result0, result0);

			mul(*tree->polys[depth-1][polyIndex], *tree->polys[depth-1][polyIndex+2], result1);
			mul(r1, result1, result1);

			mul(*tree->polys[depth-1][polyIndex], *tree->polys[depth-1][polyIndex+1], result2);
			mul(r2, result2, result2);

			add(result0, result1, result);
			add(result, result2, result);
		}
	}
}


// Given a set of len points (x_i,y_i), find the degree n+1 polynomial that they correspond to
// using the precalculated polynomial tree and Lagrange denominators ( denominator_i = \prod_{j!=i} (x_i-x_j) )
// Note that: f(u) = \sum (y_i / denominator_i) * \prod_{j!=i} (u - x_j)
Poly* Poly::interpolate(Field* field, FieldElt* y, int len, PolyTree* polyTree, FieldElt* denominators) {
	// Divide to get y'_i, where f(u) = \sum y'_i * \prod_{j!=i} (u - x_j)
	FieldElt* yPrime = new FieldElt[len];
	for (int i = 0; i < len; i++) {
		field->div(y[i], denominators[i], yPrime[i]);
		//mul(divisors[i], y[i], yPrime[i]);
	}

	Poly* result = new Poly(field);	
	interpolateRecursive(field, yPrime, polyTree, *result, len, polyTree->height - 1, 0);

	delete [] yPrime;
	return result;
}

FieldElt* Poly::genLagrangeDenominators(Field* field, Poly& func, PolyTree* tree, FieldElt* x, int len) {
	// Differentiate P_{k0}
	Poly fPrime(field);
	firstDerivative(func, fPrime);

	// Multipoint eval P'_{k0} at x_i to obtain the Lagrange denominators, where denominator_i = \prod_{j!=i} (x_i-x_j)
	FieldElt* denominators = new FieldElt[len];
	multiEval(fPrime, x, len, denominators, tree, 0, 0, tree->height - 1);

	/*	// Don't bother with this.  Be lazy.  Let the interpolate step do the division
	// Invert to get divisor_i = \prod_{j!=i} [1 / (x_i-x_j)]
	FieldElt uno;
	one(uno);
	for (int i = 0; i < len; i++) {
		div(uno, divisors[i], divisors[i]);
	}
	*/

	return denominators;
}

// Given a set of len points (x_i,y_i), find the degree len+1 polynomial that they correspond to
// Implements Algorithm 2 from http://www.mpi-inf.mpg.de/~csaha/lectures/lec6.pdf
Poly* Poly::interpolate(Field* field, FieldElt* x, FieldElt* y, int len) {
	PolyTree* tree = new PolyTree(field, x, len);

	FieldElt* denominators = genLagrangeDenominators(field, *tree->polys[tree->height-1][0], tree, x, len);

	Poly* result = interpolate(field, y, len, tree, denominators);
	
	// Cleanup
	delete tree;
	delete [] denominators;

	return result;
}

Poly* Poly::interpolateSlow(Field* field, FieldElt* x, FieldElt* y, int len) {
	Poly* result = new Poly(field);
	FieldElt* coefficients = result->getCoefficientArray(len-1);

	int degree = len - 1;
	field->zero(coefficients, degree);
	
	for (int i = 0; i < degree; i++) {
		// Compute the coefficient representation of L_i(x)
		FieldElt* lagrangeCoeff = genLagrangeCoeffs(field, x, len, i);

		// Apply them to the running tally of coefficient values
		for (int j = 0; j < degree; j++) {
			FieldElt tmp;
			field->mul(lagrangeCoeff[j], y[i], tmp);
			field->add(coefficients[j], tmp, coefficients[j]);	// hCoeff[j] += denseH[i]*lCoeff[j]
		}

		delete [] lagrangeCoeff;
	}
	
	result->setCoefficients(coefficients, len-1);

	return result;
}

// Computes the coefficient representation of the Lagrange basis polynomial indicated by index
// Observe that: L_index(x) = \prod_{i != index} 1/(s_index - s_i) * \prod_{i != index} (x - s_i)
// The left term has no dependence on x, so we can compute that separately and then apply to the
// coefficients that come from the right term
FieldElt* Poly::genLagrangeCoeffs(Field* field, FieldElt* evalPts, int len, int index) {
	FieldElt zero, tmp;
	field->zero(zero);
	FieldElt* coefficients = new FieldElt[len];
	field->zero(coefficients, len);
	
	field->one(coefficients[0]);	
	// Compute (x-evalPt[0])(x-evalPt[1]) ... (x-evalPt[d]), one term at a time
	int rootCount = 0;	// Separate from i, since we skip one of the roots
	for (int i = 0; i < len; i++) { // For each root
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
	for (int i = 0; i < len; i++) { // For each root
		if (i == index) {
			continue;	
		} 
		field->sub(evalPts[index], evalPts[i], tmp);
		field->mul(tmp, prod, prod);		
	}

	// Apply the divisor to all of the coefficients
	for (int i = 0; i < len; i++) { // For each root
		field->div(coefficients[i], prod, coefficients[i]);	
	}

	return coefficients;
}


void Poly::Initialize(Field* field) {
	pbigctx = &BignumCtx;
	memset(pbigctx, 0, sizeof(bigctx_t));

	modfft_init(field->msfield->modulo, &fftinfo, PBIGCTX_PASS);
}

void Poly::Cleanup() {
	modfft_uninit(&fftinfo, PBIGCTX_PASS);
}
