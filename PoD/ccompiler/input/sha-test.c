#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "qsp-test.h"
#include "sha-ifc.h"

const char* test_name() { return "sha"; }

RAND_CORE

void outsource_reference(struct Input *input, struct Output *output)
{
	char *sha_in_fn = "build/sha1-input.bin";
	FILE* file;
	// Write out a file we can test with sha1sum
	file = fopen(sha_in_fn, "wb");
	//fwrite("abc", sizeof(char), 3, file);
	int i;
	for (i = 0; i < INPUT_SIZE - 3; i++)
	{
		unsigned char* charArr = (unsigned char*)&input->msg[i];
		unsigned int reverse = charArr[3] <<  0 | charArr[2] <<  8
		                     | charArr[1] << 16 | charArr[0] << 24;
		//printf("%x -> %x, %x %x %x %x\n", input->msg[i], reverse, charArr[3], charArr[2], charArr[1], charArr[0]);
		fwrite(&reverse, sizeof(unsigned int), 1, file);
	}
	fclose(file);

	char *sha_out_fn = "build/sha1-output.bin";
	char cmd[100];
	snprintf(cmd, sizeof(cmd), "sha1sum %s > %s", sha_in_fn, sha_out_fn);
	int rc;
	rc = system(cmd);
	assert((rc&0xff) == 0);

	FILE* ifp = fopen(sha_out_fn, "rb");
	char buf[200];
	char* line = fgets(buf, sizeof(buf), ifp);
	fclose(ifp);
	assert(line!=NULL);

	uint8_t sha_result[20];
	scan_io_value(line, sha_result, sizeof(sha_result));
	uint32_t* sha_result_words = (uint32_t*) sha_result;
	for (i=0; i<5; i++)
	{
		output->h[i] = sha_result_words[4-i];
	}
}
  
uint32_t test_core(int iter)
{
	struct Input  input;
	struct Output output;

	int i;
	for (i=0; i<INPUT_SIZE-3; i++)
	{
		input.msg[i] = (i+iter)<<(iter*3);
	}
	for (i=INPUT_SIZE-3; i<INPUT_SIZE; i++)
	{
		input.msg[i] = 0;
	}

	// Use the string "abc"
	//input.msg[0] = 0x61626300;

	OUTSOURCE(&input, &output);

	uint32_t checksum = 0;
	for (i=0; i<OUTPUT_SIZE; i++)
	{
		checksum = (checksum*131) + output.h[i];
	}

#if DBG>0
	for (i=0; i<output.debug_count; i++)
	{
		if ((i%8)==0) { printf("\n"); }
		printf("%08x ", output.debug[i]);
		checksum = (checksum*131) + output.debug[i];
	}
	printf("\n");
#endif

	return checksum;
}

void print_output(char* buf, int buflen, struct Output* output)
{
	buf[0] = '\0';
	char tmp[16];
	int i;
	for (i=0; i<OUTPUT_SIZE; i++)
	{
		sprintf(tmp, "%08x", output->h[i]);
		strncat(buf, tmp, buflen);
	}
}
