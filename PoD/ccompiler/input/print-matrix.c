#include <stdio.h>
#include <string.h>

#define max(a,b)	((a)>(b)?(a):(b))

void buf_accum(char* buf, int buflen, char* suffix)
{
	int avail = buflen - 1 - strlen(buf);
	strncat(buf, suffix, avail);
}

void buf_close(char* buf, int buflen)
{
	if (strlen(buf) > buflen - 5)
	{
		strcpy(&buf[buflen-5], "...\n\0");
	}
}

int max_cell_size(int size_x, int size_y, int* matrix)
{
	char tmp[40];
	int i,j;
	int max_len = 0;

	for (i=0; i<size_y; i++)
	{
		for (j=0; j<size_x; j++)
		{
			sprintf(tmp, "%d", matrix[i*size_x+j]);
			max_len = max(strlen(tmp), max_len);
		}
	}
	return max_len;
}

void print_matrix_xy(char* buf, int buflen, int size_x, int size_y, int* matrix)
{
	buf[0] = '\0';
	char tmp[40];
	int i,j;

	char fmt[8];
	sprintf(fmt, "%%%dd ", max_cell_size(size_x, size_y, matrix));

	for (i=0; i<size_y; i++)
	{
		for (j=0; j<size_x; j++)
		{
			sprintf(tmp, fmt, matrix[i*size_x+j]);
			buf_accum(buf, buflen, tmp);
		}
		buf_accum(buf, buflen, "\n");
	}
	buf_close(buf, buflen);
}

void print_matrix(char* buf, int buflen, int size, int* matrix)
{
	print_matrix_xy(buf, buflen, size, size, matrix);
}


