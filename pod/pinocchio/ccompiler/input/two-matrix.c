#include "two-matrix-ifc.h"

void outsource(struct Input *input, struct Output *output)
{
	int i, j, k;
	int t;
	for (i=0; i<SIZE; i+=1)
	{
		for (j=0; j<SIZE; j+=1)
		{
			t = 0;
			for (k=0; k<SIZE; k+=1)
			{
				t = t + MAT(input->a, i, k) * MAT(input->b, k, j);
			}
			MAT(output->r, i, j) = t;
		}
	}
}
