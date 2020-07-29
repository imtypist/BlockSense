#pragma once

#include "Types.h"
#include "Field.h"
#include "Poly.h"

class Field;
class Poly;

class PolyTree {
public:
	PolyTree(Field* field, FieldElt* x, int len);
	~PolyTree();

  void deleteAllButRoot();

	Poly*** polys;
	int height;
	int numBasePolys;
};
