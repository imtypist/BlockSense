#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>

const char* prog;

void usage()
{
	fprintf(stderr, "Usage: %s outfile size reduce(bits)\n", prog);
	exit(-1);
}

void check_usage(bool cond)
{
	if (!cond)
	{
		usage();
	}
}

int main(int argc, const char** argv)
{
	prog = argv[0];
#define SCHLURP()	(check_usage(argc>0), argc--, argv++, argv[0])
	const char* fn = SCHLURP();
	int size_major = atoi(SCHLURP());
	//int size_minor = atoi(SCHLURP());
	int reduce = atoi(SCHLURP());
	check_usage(argc==1);

	srandom(19);

	FILE* fp = fopen(fn, "w");
	if (fp==NULL) {
		perror("fopen");
		exit(-1);
	}

	fprintf(fp, "#define AVAIL_RANDOM_SIZE %d\n", size_major);
	fprintf(fp, "#define AVAIL_RANDOM_REDUCE %d\n", reduce);

	int maj;
	fprintf(fp, "int data[%d] = {\n", size_major);
	for (maj=0; maj<size_major; maj++)
	{
		fprintf(fp, "0x%08x,\n", random()>>reduce);
	}
	fprintf(fp, "};\n");

	fclose(fp);
	return 0;
}
