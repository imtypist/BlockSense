#include "sha-ifc.h"

#ifdef QSP_NATIVE
#include <stdio.h>
#include <string.h>
#endif

// Assumes the message is exactly 416 bits long, 
// so the value we hash is: msg || 1000...00 || len,
// where len = 416 and there are 31 zeros
// This saves us from dealing with padding for now

// Based in part on pseudocode from http://en.wikipedia.org/wiki/SHA-1
// the spec at http://tools.ietf.org/html/rfc3174,
// and the FIPS 180-2 spec: 
// http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf

u32 leftRotate(u32 val, int amount) {
#if PARAM==0
  return val;
#else
  return (val << amount) | (val >> (32 - amount));
#endif
}

#if PARAM==0
// Succeeds at 20.  Failed at 21. Failed at 30 and 40
#define NUM_ROUNDS 21
#else
#define NUM_ROUNDS 80
#endif

void outsource(struct Input *input, struct Output *output)
{
#ifdef QSP_NATIVE
	FILE* file;
#endif 
	u32* hash;
	u32 W[80];  // Expanded message
  u32 i;
	u32 tmp;
	u32 a, b, c, d, e;

	input->msg[INPUT_SIZE-1] = 0x000001A0;
  input->msg[INPUT_SIZE-2] = 0;     // (length is 64 bits)
  input->msg[INPUT_SIZE-3] = 0x80000000; // Fix the padding
#if PARAM==0
  input->msg[INPUT_SIZE-4] = 0x1; 
  input->msg[INPUT_SIZE-5] = 0x1; 
  input->msg[INPUT_SIZE-6] = 0x1; 
  input->msg[INPUT_SIZE-7] = 0x1; 
  input->msg[0] = 0x1; 
  input->msg[1] = 0x1; 
  input->msg[2] = 0x1; 
  input->msg[3] = 0x1; 
  input->msg[4] = 0x1; 
  input->msg[5] = 0x1; 
  input->msg[6] = 0x1; 
  input->msg[7] = 0x1; 
#endif

#ifdef QSP_NATIVE
  // Write out padded input 
  file = fopen("sha-input-padded.bin", "wb");
	for (int i = 0; i < INPUT_SIZE; i+=1) {
		char* charArr = (char*)&input->msg[i];
		int reverse = charArr[3] <<  0 | charArr[2] <<  8 | 
		              charArr[1] << 16 | charArr[0] << 24;
  	fwrite(&reverse, sizeof(int), 1, file);
	}
  //fwrite(input->msg, sizeof(char), (INPUT_SIZE)*sizeof(u32), file);
  fwrite(input->msg, sizeof(u32), INPUT_SIZE, file);
  fclose(file);
#endif

  for (i = 0; i < 16; i+=1) {
    W[i] = input->msg[i];
  }

  // Expand the message further
  for (i = 16; i < NUM_ROUNDS; i+=1) {
#if PARAM==0
    //u32 tmp = W[i-16];
    //u32 tmp = W[3] ^ W[8]; 
    u32 tmp = W[i-3] ^ W[i-8]; 
    //u32 tmp = W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16];
#else
    u32 tmp = W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16];
#endif
    W[i] = leftRotate(tmp, 1);
  }

  // Initialize the state variables
  hash = output->h;
	
	hash[0] = 0x67452301;
	hash[1] = 0xEFCDAB89;
	hash[2] = 0x98BADCFE;
	hash[3] = 0x10325476;
	hash[4] = 0xC3D2E1F0;

  a = hash[0];
  b = hash[1];
  c = hash[2];
  d = hash[3];
  e = hash[4];

  for (i = 0; i < NUM_ROUNDS; i+=1) {
    u32 f;
    u32 k;

#if PARAM==0
    if (i < 23) {
      f = (b & c);
#else
    if (i < 20) {
      f = (b & c) | ((~b) & d);
#endif
      k = 0x5A827999;
    } else if (i < 40) {
      f = b ^ c ^ d;
      k = 0x6ED9EBA1;
    } else if (i < 60) {
      f = (b & c) | (b & d) | (c & d);
      k = 0x8F1BBCDC;
    } else if (i < 80) {
      f = b ^ c ^ d;
      k = 0xCA62C1D6;
    }

#if PARAM==0
    tmp = f + W[i];
#else
    tmp = leftRotate(a, 5) + f + e + k + W[i];
#endif
    e = d;
    d = c;
    c = leftRotate(b, 30);
    b = a;
    a = tmp;
  }

#if PARAM==0
  hash[0] = a;
  hash[1] = b;
  hash[2] = c;
  hash[3] = d;
  hash[4] = e;
#else
  hash[0] = hash[0] + a;
  hash[1] = hash[1] + b;
  hash[2] = hash[2] + c;
  hash[3] = hash[3] + d;
  hash[4] = hash[4] + e;
#endif
}


#ifdef _WIN32
#include <assert.h>
#include <stdio.h>
#include <string.h>

void main () {
	struct Input  input;
	struct Output output;
	int i, iter;
	char *sha_in_fn = "win-input.bin";
	FILE* file;

	for (iter = 4; iter < 10; iter++) {
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


		// Write out a file we can test with sha1sum
		file = fopen(sha_in_fn, "wb");
		//fwrite("abc", sizeof(char), 3, file);
		for (i = 0; i < INPUT_SIZE - 3; i++)
		{
			unsigned char* charArr = (unsigned char*)&input.msg[i];
			unsigned int reverse = charArr[3] <<  0 | charArr[2] <<  8
				| charArr[1] << 16 | charArr[0] << 24;
			//printf("%x -> %x, %x %x %x %x\n", input->msg[i], reverse, charArr[3], charArr[2], charArr[1], charArr[0]);
			fwrite(&reverse, sizeof(unsigned int), 1, file);
		}
		fclose(file);


		outsource(&input, &output);

		for (i=0; i<OUTPUT_SIZE; i++)
		{
			printf("%08x", output.h[i]);
		}
		printf("\n");
	}

	getc(stdin);
}
#endif
