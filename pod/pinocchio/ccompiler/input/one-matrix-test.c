#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "qsp-test.h"
#include "one-matrix-ifc.h"
#include "print-matrix.h"

RAND_CORE

void outsource_reference(struct Input *input, struct Output *output)
{
	assert(0);
}

const char* test_name() { return "one-matrix"; }

uint32_t test_core(int iter)
{
	struct Input input;
	struct Output output;
	int i, j;
	for (i=0; i<SIZE; i++)
	{
		input.v[i] = i + 7 + iter;
	}
	OUTSOURCE(&input, &output);

	uint32_t checksum = 0;
	for (i=0; i<SIZE; i++)
	{
		checksum = (checksum*131) + output.r[i];
	}
	return checksum;
}

void print_output(char* buf, int buflen, struct Output* output)
{
	print_matrix_xy(buf, buflen, 1, SIZE, output->r);
}
