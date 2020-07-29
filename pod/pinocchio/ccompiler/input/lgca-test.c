#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "qsp-test.h"
#include "lgca-ifc.h"
#include "print-matrix.h"

RAND_CORE

const char* test_name() { return "lgca"; }

void outsource_reference(struct Input *input, struct Output *output)
{
	assert(0);
}

void init_input(int iter, struct State *input)
{
	int x, y, t;
	
	srand(iter + 1);
	
	for(x=0; x<XMAX; x++)
	{
		for(y=0; y<YMAX; y++)
		{
			for (t=0; t<DEPTH; t++)
			{
				MATtyx(input->ca, t, y, x) = rand()&1;
			}
		}
	}
}

uint32_t hash131(int* buf, int count)
{
	int i;
	int sum = 0;
	for (i=0; i<count; i++)
	{
		sum = (sum*131) + buf[i];
	}
	return sum;
}

void print_state(const char* label, struct State* s)
{
	printf("\n** %s\n", label);
	char buf[1000];
	int k;
	for (k=0; k<DEPTH; k++)
	{
		print_matrix_xy(buf, sizeof(buf), XMAX, YMAX, &MATtyx(s->ca, k, 0, 0));
		printf("Depth %d:\n%s", k, buf);
	}
}

uint32_t test_core(int iter)
{
    struct State input_state;
	struct Input input;
	struct Output output;
	init_input(iter, &input_state);

#if COMPRESS
    compress(&input_state, &input.cs);

    // Check that compress/uncompress is working.
    {
        struct State state_check;
        uncompress(&input.cs, &state_check);
        int cmp = memcmp(&input_state, &state_check, sizeof(input_state));
        assert(cmp==0);
    }
#else
    input.s = input_state;
#endif // COMPRESS
	
//	print_state("Before iterations", &input.cs);
	
	OUTSOURCE(&input, &output);
	
//	print_state("After iterations", &output.cs);

    struct State output_state;
#if COMPRESS
    uncompress(&output.cs, &output_state);
#else
    output_state = output.s;
#endif // COMPRESS

	return hash131(output_state.ca, DEPTH*YMAX*XMAX);
}

void print_output(char* buf, int buflen, struct Output* output)
{
    struct State output_state;
#if COMPRESS
    uncompress(&output->cs, &output_state);
#else
    output_state = output->s;
#endif // COMPRESS

	int k=0;
	print_matrix_xy(buf, buflen, XMAX, YMAX*DEPTH, &MATtyx(output_state.ca, k, 0, 0));
}
