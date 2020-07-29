#if PARAM==0
#elif PARAM==1
#else
#error unknown PARAM
#endif

// Number of input words.  Top 65 bits are overwritten
// (512 is bits; 32 is bits-per-word.)
#define INPUT_SIZE	512 / 32  

// Number of words in a SHA-1 hash
#define OUTPUT_SIZE 160 / 32

#define u32 unsigned int

struct Input {
	u32 msg[INPUT_SIZE];
};

struct Output {
#define DBG 0
#if DBG>0
	u32 debug_count;
	u32 debug[80];
#define DBG_NEUTER 1
#else
#endif
	u32 h[OUTPUT_SIZE];
};

void outsource(struct Input *input, struct Output *output);
