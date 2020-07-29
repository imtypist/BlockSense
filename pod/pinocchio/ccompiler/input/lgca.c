#if QSP_TEST
#include <stdio.h>
#endif //QSP_TEST
#include "lgca-ifc.h"


#define RANDOM_SIZE (YMAX*XMAX)
#define RANDOM_REDUCE 30

#include "random-pragma.h"

#define matrix_embedded_random_bits data

void move(int *x, int *y) {
	int n, m;
	
	n = *x;
	m = *y;
	
	*x = n&m;
	*y = n^m;
}

#define MATxyt(m,x,y,t) MATtyx(m,t,y,x)
void propagate(struct State *state) {
	
	int i, j, k;
	int x, y;

	for (x=1; x<XMAX-1; x+=1)
	{
		for (y=1; y<YMAX-1; y+=1)
		{
			move(&MATxyt(state->ca, x, y, 0), &MATxyt(state->ca,   x, y+1, 0));
			move(&MATxyt(state->ca, x, y, 1), &MATxyt(state->ca, x+1, y+1, 1));
			move(&MATxyt(state->ca, x, y, 2), &MATxyt(state->ca, x+1,   y, 2));
			move(&MATxyt(state->ca, x, y, 3), &MATxyt(state->ca,   x, y-1, 3));
			move(&MATxyt(state->ca, x, y, 4), &MATxyt(state->ca, x-1, y-1, 4));
			move(&MATxyt(state->ca, x, y, 5), &MATxyt(state->ca, x-1,   y, 5));
		}
	}

	for (x=1; x<XMAX-1; x+=1)
	{
		move(&MATxyt(state->ca, x, 0, 0), &MATxyt(state->ca,   x, 1, 0));
		move(&MATxyt(state->ca, x, 0, 1), &MATxyt(state->ca, x+1, 1, 1));
		move(&MATxyt(state->ca, x, 0, 2), &MATxyt(state->ca, x+1, 0, 2));
		move(&MATxyt(state->ca, x, 0, 5), &MATxyt(state->ca, x-1, 0, 5));
		
		move(&MATxyt(state->ca, x, YMAX-1, 2), &MATxyt(state->ca, x+1, YMAX-1, 2));
		move(&MATxyt(state->ca, x, YMAX-1, 3), &MATxyt(state->ca,   x, YMAX-2, 3));
		move(&MATxyt(state->ca, x, YMAX-1, 4), &MATxyt(state->ca, x-1, YMAX-2, 4));
		move(&MATxyt(state->ca, x, YMAX-1, 5), &MATxyt(state->ca, x-1, YMAX-1, 5));
		
	}

	
	for (y=1; y<YMAX-1; y+=1) {
		
		move(&MATxyt(state->ca, 0, y, 0), &MATxyt(state->ca, 0, y+1, 0));
		move(&MATxyt(state->ca, 0, y, 1), &MATxyt(state->ca, 1, y+1, 1));
		move(&MATxyt(state->ca, 0, y, 2), &MATxyt(state->ca, 1,   y, 2));
		move(&MATxyt(state->ca, 0, y, 3), &MATxyt(state->ca, 0, y-1, 3));
		
		move(&MATxyt(state->ca, XMAX-1, y, 0), &MATxyt(state->ca, XMAX-1, y+1, 0));
		move(&MATxyt(state->ca, XMAX-1, y, 3), &MATxyt(state->ca, XMAX-1, y-1, 3));
		move(&MATxyt(state->ca, XMAX-1, y, 4), &MATxyt(state->ca, XMAX-2, y-1, 4));
		move(&MATxyt(state->ca, XMAX-1, y, 5), &MATxyt(state->ca, XMAX-2,   y, 5));
		
	}
	

	move(&MATxyt(state->ca, 0, 0, 0), &MATxyt(state->ca, 0, 1, 0));
	move(&MATxyt(state->ca, 0, 0, 1), &MATxyt(state->ca, 1, 1, 1));
	move(&MATxyt(state->ca, 0, 0, 2), &MATxyt(state->ca, 1, 0, 2));
	
	move(&MATxyt(state->ca, 0, YMAX-1, 2), &MATxyt(state->ca, 1, YMAX-1, 2));
	move(&MATxyt(state->ca, 0, YMAX-1, 3), &MATxyt(state->ca, 0, YMAX-2, 3));
	
	move(&MATxyt(state->ca, XMAX-1, 0, 0), &MATxyt(state->ca, XMAX-1, 1, 0));
	move(&MATxyt(state->ca, XMAX-1, 0, 5), &MATxyt(state->ca, XMAX-2, 0, 5));
	
	move(&MATxyt(state->ca, XMAX-1, YMAX-1, 3), &MATxyt(state->ca, XMAX-1, YMAX-2, 3));
	move(&MATxyt(state->ca, XMAX-1, YMAX-1, 4), &MATxyt(state->ca, XMAX-2, YMAX-2, 4));
	move(&MATxyt(state->ca, XMAX-1, YMAX-1, 5), &MATxyt(state->ca, XMAX-2, YMAX-1, 5));
}

/* THIS update() CODE QUOTED FROM:
	Deiter A. Wolf-Gladrow
	Lattice-Gas Cellular Automata and Lattice Boltzmann Models - An Introduction
	page 59-60 (pg 68 of PDF file)
*/
 
void update(struct State *state)
{
	int a,b,c,d,e,f;
	int db1,db2,db3;
	int triple;
	int xi, noxi, nsbit;
	int cha,chb,chc,chd,che,chf;
	int x,y;

	/* loop over all sites */
	for(x=0; x<XMAX; x+=1)
	{
		for(y=0; y<YMAX; y+=1)
		{
			a = MATtyx(state->ca,0,y,x);
			b = MATtyx(state->ca,1,y,x);
			c = MATtyx(state->ca,2,y,x);
			d = MATtyx(state->ca,3,y,x);
			e = MATtyx(state->ca,4,y,x);
			f = MATtyx(state->ca,5,y,x);
			/* two-body collision
			   <-> particles in cells a (b,c) and d (e,f)
			       no particles in other cells
			   <-> db1 (db2,db3) = 1
			*/
			db1 = (a&d&!(b|c|e|f));
			db2 = (b&e&!(a|c|d|f));
			db3 = (c&f&!(a|b|d|e));
			/* three-body collision <-> 0,1 (bits) alternating
				<-> triple = 1 */
			triple = (a^b)&(b^c)&(c^d)&(d^e)&(e^f);
			/* change a and d */

			xi = matrix_embedded_random_bits[y*XMAX + x] & 1; /* random bits */
			noxi = !xi; 

			// nsbit = nsb[x][y]; /* non solid bit */
			nsbit = 1;
			cha=((triple|db1|(xi&db2)|(noxi&db3))&nsbit);
			chd=((triple|db1|(xi&db2)|(noxi&db3))&nsbit);
			chb=((triple|db2|(xi&db3)|(noxi&db1))&nsbit);
			che=((triple|db2|(xi&db3)|(noxi&db1))&nsbit);
			chc=((triple|db3|(xi&db1)|(noxi&db2))&nsbit);
			chf=((triple|db3|(xi&db1)|(noxi&db2))&nsbit);
			
			/* change: a = a ^ chad */
			MATtyx(state->ca,0,y,x)=MATtyx(state->ca,0,y,x)^cha;
			MATtyx(state->ca,1,y,x)=MATtyx(state->ca,1,y,x)^chb;
			MATtyx(state->ca,2,y,x)=MATtyx(state->ca,2,y,x)^chc;
			MATtyx(state->ca,3,y,x)=MATtyx(state->ca,3,y,x)^chd;
			MATtyx(state->ca,4,y,x)=MATtyx(state->ca,4,y,x)^che;
			MATtyx(state->ca,5,y,x)=MATtyx(state->ca,5,y,x)^chf;
			
			/* collision finished */
		}
	}
}

#if 0
void bool_copy(struct State* src, struct State* dst)
{
	// Drop all but LSB of input so compiler can use 1-bit bool math
	int x, y, t;
	
	for (x=0; x<XMAX; x+=1)
	{
		for (y=0; y<YMAX; y+=1)
		{
			for (t=0; t<DEPTH; t+=1)
			{
				MATtyx(dst->ca, t, y, x) = MATtyx(src->ca, t, y, x) & 1;
			}
		}
	}
}
#endif

#if COMPRESS
void uncompress(struct CompressedState* cs, struct State* s)
{
	int x, y, t;
	for (x=0; x<XMAX; x+=1)
	{
		for (y=0; y<YMAX; y+=1)
		{
			for (t=0; t<DEPTH; t+=1)
			{
				MATtyx(s->ca, t, y, x) = CMATtyx(cs->cca, t, y, x);
			}
		}
	}
}

void clear_compressed_state(struct CompressedState* cs)
{
	int i;
	for (i=0; i<COMPRESSED_WORDS; i+=1)
	{
		cs->cca[i] = 0;
	}
}

void compress(struct State* s, struct CompressedState* cs)
{
	clear_compressed_state(cs);
	int x, y, t;
	for (t=0; t<DEPTH; t+=1)
	{
		for (y=0; y<YMAX; y+=1)
		{
			for (x=0; x<XMAX; x+=1)
			{
#if 0 && QSP_TEST
				printf("t %d y %d x %d goes to cs word %d bit %d\n",
					t, y, x,
					CMAT_WORD(t,y,x),
					CMAT_BIT(t,y,x));
#endif //QSP_TEST
				CMATtyx_set(cs->cca, t, y, x, MATtyx(s->ca, t, y, x));
			}
		}
	}
}
#endif // COMPRESS

void bool_copy(struct State* src, struct State* dst)
{
	// Drop all but LSB of input so compiler can use 1-bit bool math
	int x, y, t;
	
	for (x=0; x<XMAX; x+=1)
	{
		for (y=0; y<YMAX; y+=1)
		{
			for (t=0; t<DEPTH; t+=1)
			{
				MATtyx(dst->ca, t, y, x) = MATtyx(src->ca, t, y, x) & 1;
			}
		}
	}
}

void outsource(struct Input *in, struct Output *out)
{
	struct State system;
#if COMPRESS
	uncompress(&in->cs, &system);
#else
	bool_copy(&in->s, &system);
#endif // COMPRESS

	int i;
	for (i=0; i<ITERATIONS; i+=1)
	{
		update(&system);
		propagate(&system);
	}

#if COMPRESS
	compress(&system, &out->cs);
#else
	out->s = system;
#endif // COMPRESS
}
