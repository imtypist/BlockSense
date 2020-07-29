#include <assert.h>
#include <stdio.h>
#ifdef WIN32
#include <stdlib.h>
#define htobe64(x) _byteswap_uint64(x)
#define be64toh(x) _byteswap_uint64(x)
//#define htole64(x) (x)
//#define le64toh(x) (x)
#define bool int
#define false 0
#define true 1
#else
#include <endian.h>
#include <stdbool.h>
#endif

#include "wire-io.h"

static char int_to_hexnybble(uint32_t x)
{
	if (x<10) { return '0'+x; }
	return 'a' + (x-10);
}

static uint32_t hexnybble_to_int(char c)
{
	if (c>='0' && c<='9') { return c-'0'; }
	if (c>='A' && c<='F') { return c-'A'+10; }
	if (c>='a' && c<='f') { return c-'a'+10; }
	assert(false);
	return 0;
}

static bool is_nybble(char c)
{
	return (c>='0' && c<='9')
		|| (c>='A' && c<='F')
		|| (c>='a' && c<='f');
}

void emit_io_value(char* str, int str_len, uint8_t* value, int value_len_bytes)
{
	int si = 0;
	int vi;
	for (vi=value_len_bytes-1; vi>=0; vi--)
	{
		assert(si < str_len-2);	// room for two chars plus terminator
		str[si++] = int_to_hexnybble((value[vi]>>4)&0x0f);
		str[si++] = int_to_hexnybble((value[vi]   )&0x0f);
	}
	assert(si < str_len);
	str[si++] = '\0';
}

void scan_io_value(const char* str, uint8_t* value, int value_len_bytes)
{
	int str_end;
	int si = 0; 
	int vi;
	for (str_end=0; is_nybble(str[str_end]); str_end++)
		{ }
	assert(str_end!=0);
	si = str_end-1;
	for (vi=0; vi < value_len_bytes; vi++)
	{
		uint8_t lo_value = 0;
		uint8_t hi_value = 0;
		if (si>=0)
		{
			lo_value = hexnybble_to_int(str[si]);
			si--;
		}
		if (si>=0)
		{
			hi_value = hexnybble_to_int(str[si]);
			si--;
		}
		value[vi] = (hi_value<<4) | lo_value;
	}
	assert(si==-1);	// else had more nybbles to read than value_len_bytes could store.
}
