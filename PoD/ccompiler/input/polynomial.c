#include "polynomial-ifc.h"


#define RANDOM_SIZE (COEFFICIENTS)
#define RANDOM_REDUCE 0
#include "random-pragma.h"
#define coefficients data

int increment_exps(int *exps)
{
	int going = 1;
	int v;
	for (v=0; v<VARIABLES && going; v+=1)
	{
		if (exps[v]<DEGREE)
		{
			exps[v] += 1;
			going = 0;
		}
		else
		{
			exps[v] = 0;
			// and carry.
		}
	}
	return going;
}

void outsource(struct Input *in, struct Output *out)
{
	int d, v;
	int exponentiated_vars[VARIABLES][DEGREE+1];

	// precompute all the exponents
	for (v=0; v<VARIABLES; v+=1)
	{
		exponentiated_vars[v][0] = 1;
		int v0 = in->x[v];
		for (d=1; d<=DEGREE; d+=1)
		{
			exponentiated_vars[v][d] = v0;
			v0 = v0 * in->x[v];
		}
	}

	// exps is a vector-valued iterator that scans through all
	// the combinations of exponents:
	// [0,0,0], [1,0,0], [2,0,0], [0,1,0], ... [2,2,2].
	int exps[VARIABLES];
	for (v=0; v<VARIABLES; v+=1)
	{
		exps[v] = 0;
	}

	int total = 0;
	int done = 0;
	int coeff_idx = 0;
	while (done==0)
	{
		int term = coefficients[coeff_idx];
		for (v=0; v<VARIABLES; v+=1)
		{
			term *= exponentiated_vars[v][exps[v]];
		}
		total += term;
		coeff_idx += 1;
		done = increment_exps(exps);
	}

	out->y = total;
}
