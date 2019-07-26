#include "PolyTree.h"
#include <stdio.h>

#include "Util.h"


PolyTree::PolyTree(Field* field, FieldElt* x, int len) {
	// Precompute P_{ij}, where P_{0j} = (x - x_j) and P_{i+1,j} = P_{i,2j} * P_{i,2j+1}
	// All of the these polynomials are monic, so we omit the x^degree coefficient (since it's always 1)
	int numPolys = len;
	
	this->height = log2(numPolys) + 1;
	this->polys = new Poly**[this->height];
	this->numBasePolys = len;

	FieldElt nada;
	field->zero(nada);	

	this->polys[0] = new Poly*[numPolys];
	for (int i = 0; i < numPolys; i++) {
		this->polys[0][i] = new Poly(field, 1);
		field->one(this->polys[0][i]->coefficients[1]);
		field->sub(nada, x[i], this->polys[0][i]->coefficients[0]);	 // Poly_{0,i} = (x - x_i)
		//printf("\t*");
	}

	// Create the tree structure
	for (int i = 1; i < this->height; i++) {
		numPolys /= 2;
		this->polys[i] = new Poly*[numPolys];		
	}

	// Now fill it in.  Each interior node is the product of its children
	numPolys = len;
	for (int i = 0; i < this->height - 1; i++) {	// For each level of the tree
		//printf("\n");
		for (int j = 0; j < numPolys / 2; j++) {	// For each node in the next level up
			this->polys[i+1][j] = new Poly(field);
			Poly::mul(*this->polys[i][2*j], *this->polys[i][2*j + 1], *this->polys[i+1][j]);
			//printf("\t*");
		}

		if (numPolys % 2 == 1) { // We have an extra poly to deal with
			// Multiply it into the last poly
			Poly::mul(*this->polys[i+1][numPolys/2-1], *this->polys[i][numPolys-1], *this->polys[i+1][numPolys/2-1]);
		}

		numPolys /= 2;
	}
	//printf("\n");
}

void PolyTree::deleteAllButRoot() {
	int numPolys = numBasePolys;
	for (int i = 0; i < height-1; i++) {			
		for (int j = 0; j < numPolys; j++) {
			delete polys[i][j];
		}
		delete [] polys[i];
    polys[i] = NULL;
		numPolys /= 2;
	}
}

PolyTree::~PolyTree() {
	int numPolys = numBasePolys;
	for (int i = 0; i < height; i++) {			
    if (polys[i] != NULL) {
      for (int j = 0; j < numPolys; j++) {
        delete polys[i][j];
      }
      delete [] polys[i];
    }
    numPolys /= 2;
	}
	delete [] polys;
}
