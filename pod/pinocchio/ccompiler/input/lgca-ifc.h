#if PARAM==0
	#define	YMAX 3
	#define XMAX 3
	#define ITERATIONS 1
#elif PARAM==1
	#define	YMAX 7
	#define XMAX 7
	#define ITERATIONS 5
#elif PARAM==2
	#define	YMAX 7
	#define XMAX 7
	#define ITERATIONS 10
	// est 17kmul
#elif PARAM==3
	#define	YMAX 7
	#define XMAX 7
	#define ITERATIONS 20
	// 7 => est 49kmul
	// 14 => est 75kmul
	// 20 => est 104kmul
#elif PARAM==4
	#define	YMAX 7
	#define XMAX 7
	#define ITERATIONS 30
#elif PARAM==5
	#define	YMAX 7
	#define XMAX 7
	#define ITERATIONS 40
#else
#error unknown PARAM
#endif
// arith compile times and circuit sizes
// ITERATIONS 1  11s   67 kwires
// ITERATIONS 2  35s 1547 kwires
// ITERATIONS 3  59s 3046 kwires

#define DEPTH 6	/* the algorithm's 6-way symmetry; don't change! */

#define COMPRESS 1

//////////////////////////////////////////////////////////////////////////////
// uncompressed representation (one bit per int)

#define CELL_IDX(t,y,x)	((t)*YMAX*XMAX+(y)*XMAX+x)

#define MATtyx(m, t, y, x)	(m)[CELL_IDX(t,y,x)]

struct State {
	int ca[DEPTH*YMAX*XMAX];
};

//////////////////////////////////////////////////////////////////////////////
// compressed representation (BIT_WIDTH bits per int)

#if COMPRESS

#define COMPRESSED_WORDS (DEPTH*YMAX*XMAX/BIT_WIDTH+1)
	// +1 in case of round-off

#define CMAT_WORD(t,y,x)	(CELL_IDX(t,y,x) / BIT_WIDTH)
#define CMAT_BIT(t,y,x)		(CELL_IDX(t,y,x) % BIT_WIDTH)

#define CMATtyx(cm, t, y, x) \
	((((cm)[CMAT_WORD(t,y,x)]) >> CMAT_BIT(t,y,x)) & 1)

#define CMATtyx_set(cm, t, y, x, v) \
	(((cm)[CMAT_WORD(t,y,x)]) |= ((v&1) << CMAT_BIT(t,y,x)))

struct CompressedState {
	int cca[COMPRESSED_WORDS];
};

void uncompress(struct CompressedState* cs, struct State* s);
void clear_compressed_state(struct CompressedState* cs);
void compress(struct State* s, struct CompressedState* cs);
#endif // COMPRESS

//////////////////////////////////////////////////////////////////////////////

#if COMPRESS
struct Input { struct CompressedState cs; };
struct Output { struct CompressedState cs; };
#else
struct Input { struct State s; };
struct Output { struct State s; };
#endif // COMPRESS

