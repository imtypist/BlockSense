#pragma once
#include <stdint.h>

#if QSP_NATIVE

#define OUTSOURCE(pin, pout) \
	outsource(pin, pout)

#elif QSP_TEST

struct Input;
struct Output;

// test app-specific obligations
const char* test_name();
extern uint32_t test_core(int iter);
extern void print_output(char* buf, int buflen, struct Output* output);

// C algorithm implementation obligation:
extern void outsource(struct Input* in, struct Output* out);

// reference algorithm implementation; just assert if no reference impl available.
extern void outsource_reference(struct Input *input, struct Output *output);

// test app makes an upcall to OUTSOURCE
extern void qsp_outsource(int* in, int in_count, int* out, int out_count);
#define OUTSOURCE(pin, pout) \
	qsp_outsource((int*) pin, sizeof(*pin)/sizeof(int), (int*) pout, sizeof(*pout)/sizeof(int))

#define RAND_CORE \
	void rand_core(int iter) { \
		rand_core_inner(iter, sizeof(struct Input), sizeof(struct Output)); \
	} \

void rand_core_inner(int iter, int inSizeBytes, int outSizeBytes);

#else
#error No outsource mode specified
#endif
