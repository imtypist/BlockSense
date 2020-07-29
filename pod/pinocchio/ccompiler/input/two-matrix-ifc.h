#pragma once

#if PARAM==0
#define SIZE	4
#elif PARAM==1
#define SIZE	30
#elif PARAM==2
#define SIZE	50
#elif PARAM==3
#define SIZE	70
#elif PARAM==4
#define SIZE  90	
#elif PARAM==5
#define SIZE	110
#else
#error unknown PARAM
#endif

#define MAT(m, r, c)	(m)[(r)*SIZE+(c)]

struct Input {
	int a[SIZE*SIZE];
	int b[SIZE*SIZE];
};

struct Output {
	int r[SIZE*SIZE];
};

void outsource(struct Input *input, struct Output *output);
