#include <assert.h>
#include <intrin.h>

#pragma intrinsic(_BitScanReverse)

// Computes floor(log_2(x))
unsigned long log2(unsigned long x) {
	assert(x > 0);
	unsigned long ret;
	_BitScanReverse(&ret, x);	// Finds first 1 bit, starting from msb
	return ret;
}
