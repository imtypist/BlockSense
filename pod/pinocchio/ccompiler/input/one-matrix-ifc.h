#pragma once

#if PARAM==0  // Used for debugging
#define SIZE 	2
#elif PARAM==1
#define SIZE	200
#elif PARAM==2
#define SIZE	400
#elif PARAM==3
#define SIZE	600
#elif PARAM==4
#define SIZE	800
#elif PARAM==5
#define SIZE	1000
#elif PARAM==99  // Used for demo purposes
#define SIZE	3
#else
#error unknown PARAM
#endif

#define MAT(m, r, c)	(m)[(r)*SIZE+(c)]

// extern int data[SIZE*SIZE]; // can't declare this a second time.

struct Input {
	int v[SIZE];
};

struct Output {
	int r[SIZE];
};

void outsource(struct Input *input, struct Output *output);
