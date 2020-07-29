#pragma once

#ifdef BIGINT
#include "Field.h"
#define int FieldElt
#endif

#if PARAM==0
#define NUM_V		4
#elif PARAM==1
#define NUM_V		8
#elif PARAM==2
#define NUM_V		12
#elif PARAM==3
#define NUM_V		16
#elif PARAM==4
#define NUM_V		20
#elif PARAM==5
#define NUM_V		24
#else
#error unknown PARAM
#endif

#define EDGE_WEIGHT(weight_table, i, j)	weight_table[i*NUM_V+j]

struct Input {
	int edge_table[NUM_V*NUM_V];
};

struct Output {
	int path_table[NUM_V*NUM_V];
};

#ifdef BIGINT
void outsource(Field* field, struct Input *in, struct Output *out);
#else
void outsource(struct Input *in, struct Output *out);
#endif
