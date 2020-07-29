#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "qsp-test.h"
#include "floyd-ifc.h"

RAND_CORE

void outsource_reference(struct Input *input, struct Output *output)
{
	assert(0);
}

const char* test_name() { return "floyd"; }

#define MAX_EDGE 0x3fffffff

uint32_t test_core(int iter)
{
	srandom(iter);
	struct Input input;
	struct Output output;
	int i, j;
	for (i=0; i<NUM_V; i++)
	{
		for (j=0; j<NUM_V; j++)
		{
			if ((random() % NUM_V) < 2)
			{
				EDGE_WEIGHT(input.edge_table, i, j) = (random() % 100) + 1;
			}
			else
			{
				EDGE_WEIGHT(input.edge_table, i, j) = MAX_EDGE;
			}
		}
	}
//	{ char tmp[200]; print_matrix(tmp, sizeof(tmp), input.edge_table); printf("%s", tmp); }
	OUTSOURCE(&input, &output);

	uint32_t checksum = 0;
	for (i=0; i<NUM_V; i++)
	{
		for (j=0; j<NUM_V; j++)
		{
			checksum = (checksum*131) + EDGE_WEIGHT(output.path_table, i, j);
		}
	}
	return checksum;
}

void print_output(char* buf, int buflen, struct Output* output)
{
	print_matrix(buf, buflen, NUM_V, output->path_table);
}
