#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "qsp-test.h"
#include "two-matrix-ifc.h"
#include "print-matrix.h"

RAND_CORE

void outsource_reference(struct Input *input, struct Output *output)
{
	assert(0);
}

const char* test_name() { return "two-matrix"; }

uint32_t test_core(int iter)
{
	struct Input input;
	struct Output output;
	int i, j;
	for (i=0; i<SIZE; i++)
	{
		for (j=0; j<SIZE; j++)
		{
			MAT(input.a, i, j) = i*SIZE+j;
			MAT(input.b, i, j) = j+7+iter;
		}
	}
	OUTSOURCE(&input, &output);

	uint32_t checksum = 0;
	for (i=0; i<SIZE; i++)
	{
		for (j=0; j<SIZE; j++)
		{
			checksum = (checksum*131) + MAT(output.r, i, j);
		}
	}
	return checksum;
}

void print_output(char* buf, int buflen, struct Output* output)
{
	print_matrix(buf, buflen, SIZE, output->r);
}

