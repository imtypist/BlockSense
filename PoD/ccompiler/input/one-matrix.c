#include "one-matrix-ifc.h"

#define RANDOM_SIZE (SIZE*SIZE)
#if PARAM==0 || PARAM==99  // Demo value
#define RANDOM_REDUCE 28 
#else
#define RANDOM_REDUCE 0
#endif 

#include "random-pragma.h"

void outsource(struct Input *input, struct Output *output)
{
	int i, j, k;
	int t;
	for (i=0; i<SIZE; i+=1)
	{
		t = 0;
		for (k=0; k<SIZE; k+=1)
		{
			t = t + MAT(data, i, k) * input->v[k];
		}
		output->r[i] = t;
	}
}
