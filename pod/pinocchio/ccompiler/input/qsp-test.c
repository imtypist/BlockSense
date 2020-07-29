#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <endian.h>
#include <unistd.h> /* For sleep() */
#include <math.h>

#include "wire-io.h"

// IL = Compiler's internal intermediate language: C-equivalent expression graph
// ARITH = Jon's Python code that evaluates an arithmetic circuit
// BOOLEAN = Jon's Python code that evaluates a boolean circuit
// UNIT = Bryan's windows code with circuit execution test (same circuit as ARITH)
// CRYPTO = Bryan's windows code with full crypto proofs  (same circuit as ARITH)
typedef enum {
	NONE, NATIVE, REFERENCE, IL, ARITH, BOOLEAN, UNIT, CRYPTO, ENGINE_COUNT
} Engine;
typedef struct s_testmode {
	Engine experiment_engine;
	Engine engine;
	char* emitInFile;
	bool rand_inputs;
	bool print_output;
	bool cmp_native;
	int iters;
	int arith_bit_width;
	const char* circuit_file;
	bool bitwise_io;
} Testmode;
Testmode testmode;

const char* engine_str(Engine engine)
{
	switch (engine)
	{
	case NONE:		return "none";
	case NATIVE:	return "native";
	case REFERENCE:	return "reference";
	case IL:		return "il";
	case ARITH:		return "arith";
	case BOOLEAN:	return "boolean";
	case UNIT:		return "unit";
	case CRYPTO:	return "crypto";
	default:		assert(false);
	}
}

/*******************************************
 *   Timer code (feel free to move elsewhere)
 *******************************************/
#define Sleep(x) usleep(x*1000)

typedef struct s_timer {
        uint64_t start;
        uint64_t stop;
} Timer;

double cpuFreq = 0;

uint64_t GetRDTSC() {
	uint64_t rv;
	__asm__ (
		"push		%%ebx;"
		"cpuid;"
		"pop		%%ebx;"
		:::"%eax","%ecx","%edx");
	__asm__ __volatile__ ("rdtsc" : "=A" (rv));
	return (rv);
}

void initTimers() {
	// Calibrate frequency
	uint64_t start = GetRDTSC();
	Sleep(200);  // Sleep for 200ms
	uint64_t stop = GetRDTSC();
	
	cpuFreq = (stop - start) / (200 / 1000.0);

	//cout << "Measured CPU at: " << cpuFreq << endl;
	printf("Measured CPU at %f GHz\n", cpuFreq/pow(10.0, 9));
	assert(cpuFreq);
}

void startTimer(Timer* t) {
	assert(t);
	t->start = GetRDTSC();
}

void stopTimer(Timer* t) {
	assert(t);
	t->stop = GetRDTSC();
}

double getTimeSeconds(Timer* t) {
	assert(t);
	return (t->stop - t->start) / cpuFreq;
}

uint64_t getTimeCycles(Timer* t) {
	assert(t);
	return (t->stop - t->start);
}



void emit_input(const char* ifn, int* in, int in_count, bool bitwise_io)
{
	FILE* ifp = fopen(ifn, "w");
	int wi;	// word index
	int ii = 0;	// input index (units 'bits' in BOOLEAN case)
	for (wi=0; wi<in_count; wi++)
	{
		char buf[150];
		uint32_t bevalue = htole32(in[wi]);
		if (bitwise_io)
		{
			int biti;
			for (biti=0; biti<32; biti++)
			{
				int bit_value = (bevalue>>biti) & 1;
				emit_io_value(buf, sizeof(buf), (uint8_t*) &bit_value, 1);
				fprintf(ifp, "%d %s\n", ii, buf);
				ii += 1;
			}
		} else {
			emit_io_value(buf, sizeof(buf), (uint8_t*) &bevalue, sizeof(bevalue));
			fprintf(ifp, "%d %s\n", ii, buf);
			ii += 1;
		}
	}
	fprintf(ifp, "%d %d\n", ii, 1);
	ii += 1;
	fclose(ifp);
}

void rstrip(char *str)
{
	char *p;
	for (p=str+strlen(str)-1; p>=str && (*p==' ' || *p=='\r' || *p=='\n'); p--)
	{
		*p = '\0';
	}
}

bool read_value(FILE* ofp, int* last_idx, uint32_t* out_value)
{
	char line[800];
	char* result = fgets(line, sizeof(line), ofp);
	if (result==NULL)
	{
		return false;
	}

	int idx = atoi(result);
	assert(idx > *last_idx);	// hope they come back in order. Lazy me.
	*last_idx = idx;
	char* the_space = index(result, ' ');
	assert(the_space!=NULL);
	char* value_field = the_space + 1;

	uint32_t bevalue;

	// Truncate values past 32 bits to 32 bits at output time. We don't want to force
	// the circuit to do this expensively since it's cheap for us, and we don't want to
	// force 32-bitness on all applications. We're still playing in a weird space where
	// unsigned math is 254 bits, but signed math (*-1) is 32 bits. ugh.
	rstrip(value_field);
	if (strlen(value_field)>8)
	{
		// skip most-significant nybbles.
		char* new_value_field = value_field + strlen(value_field)-8;
		//fprintf(stderr, "was %s %d scanning '%s' %d\n", value_field, strlen(value_field), new_value_field, strlen(new_value_field));
		value_field = new_value_field;
	}

	scan_io_value(value_field, (uint8_t*) &bevalue, sizeof(bevalue));
	uint32_t hvalue = le32toh(bevalue);
	*out_value = hvalue;
	return true;
}

void read_output(const char* ofn, int* out, int out_count, bool bitwise_io)
{
	FILE* ofp = fopen(ofn, "r");
	if (ofp==NULL)
	{
		fprintf(stderr, "Cannot open '%s'\n", ofn);
		exit(-1);
	}

	int oi;
	int last_idx = -1;
	for (oi=0; oi<out_count; oi++)
	{
		if (bitwise_io)
		{
			uint32_t accum = 0;
			int biti;
			for (biti=0; biti<32; biti++)
			{
				int bitv;
				bool rc = read_value(ofp, &last_idx, &bitv);
				assert(rc);
				accum |= (bitv<<biti);
			}
			out[oi] = accum;
		}
		else
		{
			bool rc = read_value(ofp, &last_idx, &out[oi]);
			if (!rc) { break; }
		}
	}
	assert(oi==out_count);
	fclose(ofp);
}

void qsp_outsource(int* in, int in_count, int* out, int out_count)
{
	if (testmode.engine==NATIVE)
	{
		outsource(in, out);
	}
	else if (testmode.engine==REFERENCE)
	{
		outsource_reference(in, out);
	}
	else
	{
		char ifn[800];
		snprintf(ifn, sizeof(ifn), "build/%s.in", test_name());
	
		char ofn[800];
		snprintf(ofn, sizeof(ofn), "build/%s.out", test_name());
	
		emit_input(ifn, in, in_count, testmode.bitwise_io);

		char basefile[80];
		char* il_suffix;
		char* arith_suffix;
		char* bool_suffix;
		if (testmode.circuit_file != NULL)
		{
			snprintf(basefile, sizeof(basefile), "%s", testmode.circuit_file);
			il_suffix = "";
			arith_suffix = "";
			bool_suffix = "";
		}
		else
		{
			snprintf(basefile, sizeof(basefile), "build/%s-p%d-b%d", test_name(), PARAM, testmode.arith_bit_width);
			il_suffix = ".il";
			arith_suffix = ".arith";
			bool_suffix = ".bool";
		}

		char cmd[800];
		if (testmode.engine==IL)
		{
			snprintf(cmd, sizeof(cmd), "/usr/bin/python ../src/dfgexec.py %s%s %s %s",
				basefile, il_suffix, ifn, ofn);
		}
		else if (testmode.engine==ARITH)
		{
			snprintf(cmd, sizeof(cmd), "/usr/bin/python ../src/aritheval.py %s%s %s %s",
				basefile, arith_suffix, ifn, ofn);
		}
		else if (testmode.engine==BOOLEAN)
		{
			snprintf(cmd, sizeof(cmd), "/usr/bin/python ../src/booleval.py %s%s %s %s",
				basefile, arith_suffix, ifn, ofn);
		}
		else if (testmode.engine==UNIT)
		{
			// Not sure how to avoid including an assumption about 
			// which directory this is run from relative to the qapv2's directory,
			// without doing some complicated path parsing
			snprintf(cmd, sizeof(cmd), "../../qapv2/windows/x64/Release/qapv2.exe --unit --file %s%s --input %s --output %s",
				basefile, arith_suffix, ifn, ofn);
		}
		else if (testmode.engine==CRYPTO)
		{
			// Not sure how to avoid including an assumption about 
			// which directory this is run from relative to the qapv2's directory,
			// without doing some complicated path parsing
			snprintf(cmd, sizeof(cmd), "../../qapv2/windows/x64/Release/qapv2.exe --qap --dv --mem=6 --file %s%s --input %s --output %s",
				basefile, arith_suffix, ifn, ofn);
		}
		else
		{
			assert(false);	 // unimplemented engine
		}
	
		 fprintf(stderr, "%s\n", cmd);
		//snprintf(cmd, sizeof(cmd), "/bin/false");
		int rc = system(cmd);
		//fprintf(stderr, "system %d\n", WEXITSTATUS(rc));
		if (rc != 0) {
			printf("FAILURE: %s returned non-zero value: %d\n", cmd, rc);
		}
		assert(rc==0);

		read_output(ofn, out, out_count, testmode.bitwise_io);
	}

	if (testmode.print_output)
	{
		char resultbuf[200];
		print_output(resultbuf, sizeof(resultbuf), out);
		fprintf(stdout, "%-10s\n%s\n", engine_str(testmode.engine), resultbuf);
	}
}

int main(int argc, char** argv)
{
	testmode.experiment_engine = ARITH;
	testmode.rand_inputs = false;
	testmode.print_output = true;
	testmode.cmp_native = true;
	testmode.iters = 1;
	testmode.arith_bit_width = 32;
	testmode.emitInFile = NULL;
	testmode.circuit_file = NULL;
	testmode.bitwise_io = false;

	argc--; argv++;
	while (argc>0)
	{
		if (strcmp(argv[0], "--engine")==0)
		{
			assert(argc>=2);

			int ei;
			for (ei=0; ei<ENGINE_COUNT; ei++)
			{
				if (strcmp(argv[1], engine_str(ei))==0)
				{
					break;
				}
			}
			if (ei==ENGINE_COUNT)
			{
				fprintf(stderr, "Unknown engine %s\n", argv[1]);
				exit(-1);
			}
			testmode.experiment_engine = ei;

			if (testmode.experiment_engine == NONE
				|| testmode.experiment_engine == NATIVE)
			{
				testmode.cmp_native = false;
			}

			if (testmode.experiment_engine == BOOLEAN)
			{
				testmode.bitwise_io = true;
			}

			argc--; argv++;
		}
		else if (strcmp(argv[0], "--circuit")==0)
		{
			testmode.circuit_file = argv[1];
			argc--; argv++;
		}
		else if (strcmp(argv[0], "--rand-inputs")==0)
		{
			testmode.rand_inputs = true;
		}
		else if (strcmp(argv[0], "--print-output")==0)
		{
			testmode.print_output = true;
		}
		else if (strcmp(argv[0], "--no-print-output")==0)
		{
			testmode.print_output = false;
		}
		else if (strcmp(argv[0], "--cmp-native")==0)
		{
			testmode.cmp_native = true;
		}
		else if (strcmp(argv[0], "--no-cmp-native")==0)
		{
			testmode.cmp_native = false;
		}
		else if (strcmp(argv[0], "--emitInFile")==0)
		{
			assert(argc>=2);
			testmode.emitInFile = argv[1];
			argc--; argv++;
		}
		else if (strcmp(argv[0], "--iters")==0)
		{
			assert(argc>=2);
			testmode.iters = atoi(argv[1]);
			argc--; argv++;
		}
		else if (strcmp(argv[0], "--arith-bit-width")==0)
		{
			// Tells us which .arith file to select.
			assert(argc>=2);
			testmode.arith_bit_width = atoi(argv[1]);
			argc--; argv++;
		}
		else
		{
			fprintf(stderr, "Unrecognized argument %s\n", argv[0]);
			exit(-1);
		}
		argc--; argv++;
	}

	if (testmode.rand_inputs) {
		rand_core(testmode.iters);
	} else {
		int iter;
		for (iter=0; iter<testmode.iters; iter++)
		{
			//printf("iter %d/%d\n", iter, testmode.iters);
			testmode.engine = testmode.experiment_engine;
			uint32_t result_experiment = -1;
				result_experiment = test_core(iter);
			if (testmode.cmp_native)
			{
				testmode.engine = NATIVE;
				uint32_t result_control = test_core(iter);
				if (result_experiment != result_control)
				{
					fprintf(stderr, "FAIL iter %d experiment disagrees with control\n", iter);
					exit(-1);
				}
			}
		}
	}
	return 0;
}

void rand_core_inner(int iter, int inSizeBytes, int outSizeBytes) {
	int inSizeWords = inSizeBytes / sizeof(int);
	int* input = (int*)malloc(inSizeBytes);
	int* output = (int*)malloc(outSizeBytes);
	int i;

	srand(iter + 42);
	for (i = 0; i < inSizeWords; i++) {
		input[i] = rand();
	}
	
	if (testmode.emitInFile != NULL) {
		emit_input(testmode.emitInFile, input, inSizeWords, testmode.bitwise_io);
	} 
		
	if (testmode.experiment_engine == NATIVE) {
		initTimers();

		Timer timer;
		startTimer(&timer);
		outsource((struct Input*)input, (struct Output*)output);
		stopTimer(&timer);

		printf("%s: %f\n", engine_str(testmode.experiment_engine), getTimeSeconds(&timer));
	} else if (testmode.engine == NONE) {
	} else {
		assert(0);		// Unknown engine
	}
}
