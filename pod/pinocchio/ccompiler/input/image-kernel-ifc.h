#pragma once

#ifdef BIGINT
#include "Field.h"
#define int FieldElt
#endif

#if PARAM==0
	#define IMAGE_W		5
	#define IMAGE_H		5
	#define KERNEL_W	2
	#define KERNEL_H	2
#elif PARAM==1
	#define IMAGE_W		5
	#define IMAGE_H		5
	#define KERNEL_W	3
	#define KERNEL_H	3
#elif PARAM==2
	#define IMAGE_W	  15
	#define IMAGE_H		15
	#define KERNEL_W	3
	#define KERNEL_H	3
#elif PARAM==3
	#define IMAGE_W		25
	#define IMAGE_H		25
	#define KERNEL_W	3
	#define KERNEL_H	3
#elif PARAM==4
	#define IMAGE_W		35
	#define IMAGE_H		35
	#define KERNEL_W	3
	#define KERNEL_H	3
#elif PARAM==5
	#define IMAGE_W		45
	#define IMAGE_H		45
	#define KERNEL_W	3
	#define KERNEL_H	3
#else
#error unknown PARAM
#endif

struct Point { int x; int y; };

#define IMAGE_PIXEL(im,p0,p1)	(im[((p0)->y+(p1)->y)*IMAGE_H+((p0)->x+(p1)->x)])
#define KERNEL_PIXEL(ke,p)		(ke[(p)->y*KERNEL_H+(p)->x])

extern int image[IMAGE_H*IMAGE_W];

struct Input {
	int kernel[KERNEL_H*KERNEL_W];
};

struct Output {
	struct Point min_loc;
	int min_delta;
};

#ifdef BIGINT
void outsource(Field* field, struct Input *in, struct Output *out);
#else
void outsource(struct Input *in, struct Output *out);
#endif
