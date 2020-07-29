#include <stdio.h>
#include <assert.h>
#include "qsp-test.h"
#include "image-kernel-ifc.h"

RAND_CORE

void outsource_reference(struct Input *input, struct Output *output)
{
	assert(0);
}

const char* test_name() { return "image-kernel"; }

uint32_t test_core(int iter)
{
	struct Input in = { .kernel = {
// a small supply of hardcoded randomness for the kernel
#if (KERNEL_W * KERNEL_H) == 4 || (KERNEL_W * KERNEL_H) == 9
0x00000004,
0x00000005,
0x00000003,
0x00000000,
#endif
#if (KERNEL_W * KERNEL_H) == 9
0x00000003,
0x0000000a,
0x00000005,
0x00000009,
0x00000004,
#endif
	} };

	struct Point a;
	for (a.y=0; a.y<KERNEL_H; a.y++)
	{
		for (a.x=0; a.x<KERNEL_W; a.x++)
		{
			KERNEL_PIXEL(in.kernel, &a) += iter;
		}
	}

	struct Output out;

	OUTSOURCE(&in, &out);

	uint32_t checksum = 0;
	checksum = (checksum*131) + out.min_loc.x;
	checksum = (checksum*131) + out.min_loc.y;
	checksum = (checksum*131) + out.min_delta;
	return checksum;
}

void print_output(char* buf, int buflen, struct Output* out)
{
	snprintf(buf, buflen, "best point: %d,%d delta %d\n",
		out->min_loc.x, out->min_loc.y, out->min_delta);
}
