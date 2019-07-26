/*
 *  Run with -h for all of the options.  Also checks config.txt for option specifications.
 */

#pragma warning( disable : 4005 )	// Ignore warning about intsafe.h macro redefinitions
#include "Types.h"
#include <iostream>
#include <fstream>
#include <boost/program_options.hpp>
#include "timer.h"
#include "Network.h"

using namespace std;
using namespace boost::program_options;

TimerList timers;
extern void runQspTests(variables_map& config);
extern void runQapTests(variables_map& config);
extern void runMicroTests(variables_map& config);
extern void runUnitTest(variables_map& config);

#ifndef __FFLIB
// Does the real work of main(), so memory-leak detection will be simpler
// (returning from here should clear everything off the stack)
void dispatch(int argc, char **argv) {
	// Parse the command-line options
	options_description genericOpt("Generic options");
	genericOpt.add_options()
			("help,h", "Produce the help message")
			("config,c", value<string>(), "Specify a config file.  Defaults to config.txt.")
			("raw", "Dump output in a raw, machine-readable format, instead of human readable")
			;

	options_description highOpt("High-level options (must choose one)");
	highOpt.add_options()
			("qsp", "Build a QSP")
			("qap", "Build a QAP")
			("micro", value<string>(), "Run microbenchmarks.  Options include: 'all', 'field', 'encoding', 'poly-all', 'poly-fast', 'poly-slow'")
			("unit,u", "Execute the circuit specified with --file.  Specify inputs with --input or use --bits for random input.  Use --output to write out circuit outputs.")
			;

	options_description cryptOpt("Crypto protocol options (must choose at least one of dv or pv -- nizk is optional)");
	cryptOpt.add_options()							
			("dv", "Designated verifier")
			("pv", "Public verifier")
			("nizk", "Enable/compute NIZKs")
			;

	options_description circuitOpt("Circuit options");
	circuitOpt.add_options()	
			("file", value<string>(), "Read circuit description from file")			
			("matrix", value<int>(), "Build an internal circuit for NxN matrix multiplication (QAP only)")
			("input", value<string>(), "The inputs to execute the circuit on")
			("bits", value<int>(), "Randomly generate inputs instead, choosing from {0,1}^bits")
			("output", value<string>(), "File to write circuit outputs to")
			;

	options_description execOpt("Execution options");
	execOpt.add_options()				
			("genkeys", "Generate keys and exit")
			("dowork", "Compute the proof and exit")
			("verify", "Verify a proof and exit")
			("server", "Run as a server")
			("client", value<string>(), "Run as a client, connecting to a server at the specified IP")
			("keys", value<string>(), "Write the keys and proof out using the filename specified")
			("trials,t", value<int>()->default_value(100), "Number of instances to use for microbenchmarks ")		
			("test", "Run a test -- used for debugging")
			;

	options_description memOpt("Memory options");
	memOpt.add_options()				
			("mem", value<int>(), "Maximum amount of memory to use for crypto tables (in GB)")
			("fill", "Precomputation is 'free', so fill all of memory with crypto tables")
			("pcache", "Cache intermediate polys during QAP construction (trades memory for speed).  Default off.")
			;

	// Group all of the options together
	options_description all("Allowed options");
	all.add(genericOpt).add(highOpt).add(cryptOpt).add(circuitOpt).add(execOpt).add(memOpt);

	variables_map config;
	store(parse_command_line(argc, argv, all), config);
	notify(config);  

	if (config.count("help")) {
		cout << all << endl;
		return;
	}

	string config_filename = "config.txt";		// Default value
	if (config.count("config")) {
		config_filename = config["config"].as<string>();
	} 

	ifstream ifs(config_filename.c_str());
  if (!ifs) {
      cout << "Warning: Could not open config file: " 
			     << config_filename 
					 << ".  Continuing based on command line only.\n";
  } else {
      store(parse_config_file(ifs, all), config);
      notify(config);
			ifs.close();
  }

	// Sanity check some of the arguments
	if ( (config.count("qap") || config.count("qsp")) &&
			 config.count("nizk") && !config.count("pv")  ) {
		cout << "DV NIZKs are not currently supported!\n";
		return;
	}

	if ((config.count("qap") || config.count("qsp")) && 
		  !config.count("pv") && !config.count("dv")    ) {
		cout << "Must choose at least one of pv or dv!\n";
		return;
	}


// Display info about the current configuration
#ifdef DEBUG_ENCODING
	printf("Using the identity encoding!\n");
#endif 

#ifdef DEBUG_RANDOM
	printf("Warning!  Random elements aren't that random!\n");
	srand(5);
#else
	unsigned int seed = (unsigned int)time(NULL);	// Save this for debugging purposes
	srand(seed);
#endif 

#ifdef _DEBUG
	printf("\nRunning in DEBUG mode -- performance measurements should not be trusted!\n\n");
#endif

#ifdef USE_OLD_PROTO
	printf("\nUsing the OLD crypto protocol!\n");
#else !USE_OLD_PROTO
	printf("\nUsing the NEW crypto protocol!\n");
#endif

//#ifndef _DEBUG	// If we're debugging, let VS catch the exceptions, so we can see where they came from
//	try {
//#endif
		if (config.count("qsp")) {
			runQspTests(config);
		} else if (config.count("qap")) {
			runQapTests(config);
		} else if (config.count("micro")) {
			runMicroTests(config);
		} else if (config.count("unit")) {
			runUnitTest(config);
		} else {
			printf("\nWarning: No high-level option recognized, so I'm not doing anything.\n");
		}
//#ifndef _DEBUG
//	} catch (exception e) {
//		cout << "Caught an exception: " << e.what() << endl;
//	} catch (...) {
//		cout << "Caught an unknown exception\n";
//	}
//#endif
	
	timers.cleanup();

}


// TODO: Find a way to restore OPTIMIZED_FUNCTIONS in pfc_bn.h?
int main(int argc, char **argv) {
	dispatch(argc, argv);

	#ifdef MEM_LEAK
	//_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	//_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDOUT);
	_CrtDumpMemoryLeaks();  // Check for memory leaks
	#endif

	printf("\nDone!\n");
#ifdef _DEBUG
	getc(stdin); // Make the VS window remain up during debugging
#endif

	return 0;
}

#else
// Interface for F# code

#include "Encoding.h"
#include "Field.h"
#include "SparsePolynomial.h"

extern "C" {

__declspec(dllexport) Encoding * makeEncoding()
{
	return new Encoding;
}

__declspec(dllexport) Field * getField(Encoding * enc)
{
	std::cerr << "Retrieving field: " << std::hex << enc->srcField << std::endl;
	return enc->srcField;
}

__declspec(dllexport) FieldElt * getRandomFieldElt(Field * f)
{
	return f->newRandomElt();
}

__declspec(dllexport) FieldElt * field_add(const FieldElt* f1, const FieldElt * f2, Field * f)
{
	FieldElt * ret = (FieldElt*)new FieldElt;
	f->add(*f1,*f2,*ret);
	return ret;
}

__declspec(dllexport) FieldElt * field_mul(const FieldElt * f1, const FieldElt * f2, Field * f)
{
	FieldElt * ret = (FieldElt *)new FieldElt;
	f->mul(*f1,*f2,*ret);
	return ret;
}

__declspec(dllexport) FieldElt * field_sub(const FieldElt * f1, const FieldElt * f2, Field * f)
{
	FieldElt * ret = (FieldElt *)new FieldElt;
	f->sub(*f1, *f2, *ret);
	return ret;
}

__declspec(dllexport) FieldElt * field_div(const FieldElt * f1, const FieldElt * f2, Field * f)
{
	FieldElt * ret = (FieldElt*)new FieldElt;
	f->div(*f1,*f2,*ret);
	return ret;
}

__declspec(dllexport) FieldElt * field_exp(const FieldElt * f1, int ex, Field * f)
{
	FieldElt * ret = (FieldElt*)new FieldElt;
	f->exp(*f1, ex, *ret);
	return ret;
}

__declspec(dllexport) bool fieldEltEql(const FieldElt * f1, const FieldElt * f2, Field * f)
{
	return f->equal(*f1,*f2);
}

__declspec(dllexport) void printFieldElt(const FieldElt * f1, Field * f)
{
	f->print(*f1);
}

__declspec(dllexport) FieldElt * makeFieldElt(int i, Field * f)
{
	FieldElt * ret = (FieldElt*)new FieldElt;
	f->set(*ret, i);
	return ret;
}

char ret[1024];

__declspec(dllexport) char * fieldEltToString(const FieldElt * f1)
{
	// Probably a bad idea...
	//char * ret = new char[1024];
#ifdef DEBUG_ENCODING
	sprintf(ret, "%lld", *f1[0]);
#else
	sprintf(ret, "%lld %lld %lld %lld", *f1[0], *f1[1], *f1[2], *f1[3]);
#endif
	return ret;
}

__declspec(dllexport) void deleteFieldElt(FieldElt * arr)
{
	// Should be able to just do this
	delete [] arr;
}

__declspec(dllexport) SparsePolynomial* makeSparsePolyArray(int size)
{
	return new SparsePolynomial[size];
}

_declspec(dllexport) void addPolyRoot(SparsePolynomial * arr, int idx, int rtID, const FieldElt * value, Field * f)
{
	assert(idx >= 0);
	assert(rtID >= 0);
	arr[idx].addNonZeroRoot(rtID, *value, f);
}

__declspec(dllexport) void deletePolyArray(SparsePolynomial * arr)
{
	delete [] arr;
}


}
#endif