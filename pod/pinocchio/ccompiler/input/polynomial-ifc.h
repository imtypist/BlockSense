#if PARAM==0
	#define DEGREE		4
	#define VARIABLES	3
	#define COEFFICIENTS 247
#elif PARAM==1
	#define DEGREE		6
	#define VARIABLES	5
	#define COEFFICIENTS 16807
#elif PARAM==2
	#define DEGREE		7
	#define VARIABLES	5
	#define COEFFICIENTS 32768
#elif PARAM==3
	#define DEGREE		8
	#define VARIABLES	5
	#define COEFFICIENTS 131022 
#elif PARAM==4
	#define DEGREE		9
	#define VARIABLES	5
	#define COEFFICIENTS 500040
#elif PARAM==5
	#define DEGREE		10
	#define VARIABLES	5
	#define COEFFICIENTS 644170
	// 38900 naive, 32421 actual, 31044 predicted, 24646 improved
// NB if we decide to go past 10,000, we'll need to increase both the
// count of values in make-random-header.py,
// and the loop_sanity_limit in vercomp.py
//
// naive amuls:
/*
python -c 'import math; d=3; v=3; print ((d-1)*v + math.pow(d+1,v)*v)'
*/
// improved amuls:
/*
python -c 'import math; d=5; v=5; print ((d-1)*v + math.pow(d+1,v)*(v-1) - v*pow(2,v-1))'
*/
#else
#error unknown PARAM
#endif


extern int coefficients[COEFFICIENTS];

struct Input {
	int x[VARIABLES];
};

struct Output {
	int y;
};

void outsource(struct Input *in, struct Output *out);

