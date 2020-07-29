#include "floyd-ifc.h"

void outsource(struct Input *in, struct Output *out)
{
	int i, j, k;
	for (i=0; i<NUM_V; i+=1)
	{
		for (j=0; j<NUM_V; j+=1)
		{
			EDGE_WEIGHT(out->path_table, i, j) = EDGE_WEIGHT(in->edge_table, i, j);
		}
	}
	for (k=0; k<NUM_V; k+=1)
	{
		for (i=0; i<NUM_V; i+=1)
		{
			for (j=0; j<NUM_V; j+=1)
			{
				int existing = EDGE_WEIGHT(out->path_table, i, j);
				int detoured = EDGE_WEIGHT(out->path_table, i, k) + EDGE_WEIGHT(out->path_table, k, j);
				int best_path = existing;
				if (detoured < existing)
				{
#if 0 && QSP_TEST
					printf("%d --(%d)-> %d --(%d)-> %d == %d < %d\n",
						i, EDGE_WEIGHT(out->path_table, i, k), k, EDGE_WEIGHT(out->path_table, k, j), j,
						detoured, existing);
#endif // QSP_TEST
					best_path = detoured;
				}
				EDGE_WEIGHT(out->path_table, i, j) = best_path;
			}
		}
	}
}
