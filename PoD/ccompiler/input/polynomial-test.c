#include <assert.h>
#include <stdio.h>
#include "qsp-test.h"
#include "polynomial-ifc.h"

RAND_CORE

void outsource_reference(struct Input *input, struct Output *output)
{
	assert(0);
}

const char* test_name() { return "polynomial"; }

uint32_t test_core(int seed)
{
	struct Input in;
	struct Output out;

	int i;
	for (i=0; i<VARIABLES; i++)
	{
		in.x[i] = seed;
	}

	OUTSOURCE(&in, &out);
}

void print_output(char* buf, int buflen, struct Output* output)
{
	snprintf(buf, buflen, "%d", output->y);
}
