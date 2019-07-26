#include "blocksense-ifc.h"

void outsource(struct Input *input, struct Output *output)
{
	int i,j;
	for (i = WINDOW_SIZE / 2; i < DATA_LENGTH - WINDOW_SIZE / 2; i+=1)
	{
		int temp[WINDOW_SIZE];
		for (j = 0; j < WINDOW_SIZE; j+=1)
		{
			temp[j] = input->t[i- WINDOW_SIZE/2 + j];
		}
		// bubbleSort
		int m,n;
		for (m = 0; m<WINDOW_SIZE - 1; m+=1){
        	for (n = 0; n < WINDOW_SIZE - m - 1; n+=1){
            	if (temp[n] > temp[n + 1]) {
	                temp[n+1] = temp[n] + temp[n+1];
	                temp[n] = temp[n+1] - temp[n];
	                temp[n+1] = temp[n+1] - temp[n];
	            }
        	}
    	}

		int median = temp[WINDOW_SIZE/2];
		if(input->t[i] < (median - SIGMA)){
			output->x[i] = 1;
		}else if(input->t[i] > (median + SIGMA)){
			output->x[i] = 1;
		}else{
			output->x[i] = 0;
		}
	}
	for (i = 0; i < WINDOW_SIZE/2; i+=1)
	{
		output->x[i] = 0;
	}
	for (i = DATA_LENGTH - WINDOW_SIZE / 2; i < DATA_LENGTH; i+=1)
	{
		output->x[i] = 0;
	}
}