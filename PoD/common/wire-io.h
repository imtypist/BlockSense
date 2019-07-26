#pragma once


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// unlimited-bit interface.
// value[0] is the least-significant byte.
// Output is written in big-endian notation, with the least-significant
// nybble written at the end of the string.
void emit_io_value(char* str, int str_len, uint8_t* value, int value_len_bytes);

// As with emit, the string is big-endian (of any length)
// and the value[] array is least-significant-byte-first (in [0]).
// In the (typical) case that the string is shorter than value_len_bytes,
// the most-significant bytes of value[] are zeroed (as you'd hope).
// If there are more nybbles in the string than fit in value_len_bytes,
// the function assert-fails.
void scan_io_value(const char* str, uint8_t* value, int value_len_bytes);

#ifdef __cplusplus
}
#endif
