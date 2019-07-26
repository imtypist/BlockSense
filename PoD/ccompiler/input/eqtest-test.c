#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "qsp-test.h"
#include "eqtest-ifc.h"

RAND_CORE

void outsource_reference(struct Input *input, struct Output *output)
{
	assert(0);
}

const char* test_name() { return "eqtest"; }

uint32_t test_core(int iter)
{
	struct Input in;
	struct Output out;

	srandom(iter);

	in.a = random();
	in.b = random();
	OUTSOURCE(&in, &out);
	return out.x;
}

void print_output(char* buf, int buflen, struct Output* output)
{
	snprintf(buf, buflen, "%08x\n%08x\n%08x\n",
		output->x);
}

