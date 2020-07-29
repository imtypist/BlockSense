/*
 *  QSP-specific Tests
 */

#pragma warning( disable : 4005 )	// Ignore warning about intsafe.h macro redefinitions
#define WIN32_LEAN_AND_MEAN  // Prevent conflicts in Window.h and WinSock.h

#include <stdio.h>
#include <time.h>
#include <assert.h>
#include <boost/program_options.hpp>
#include "Types.h"
#include "timer.h"
#include "CircuitBinaryAdder.h"
#include "Encoding.h"
#include "QSP.h"


using namespace std;
using namespace boost::program_options;

#ifdef USE_OLD_PROTO

#pragma warning( disable : 4800 )	// Ignore warning about using .count() as bool

/******  Build a QSP based on a very simple adder circuit ***/
void runSimpleQspTests(variables_map& config) {
	Encoding* encoding = new Encoding();

	CircuitBinaryAdder circuit(5);

	printf("Built a circuit with %d gates\n", circuit.gates.size());

	int i = 0;
	for (BoolWireVector::iterator iter = circuit.inputs.begin();
	     iter != circuit.inputs.end();
			 iter++) {
		(*iter)->value = i++ % 2 == 0;		
	}
	
	circuit.eval();
	
	// Try constructing a QSP
	QSP qsp(&circuit, encoding);	

	Keys* keys = qsp.genKeys(config);

	QspProof* p = qsp.doWork(keys->pk, config);

	if (qsp.verify(keys, p)) {
		printf("Simple proof checks out\n\n");
	} else {
		printf("Simple proof fails ********************\n\n");
	}

	delete encoding;
	delete keys;
	delete p;
}

/********** Run an actual performance test based on the circuit specified ***********/
void runQspPerfTest(variables_map& config) {
	printf("Using circuit from file %s\n", config["file"].as<string>().c_str());

	Timer* totalTimer = timers.newTimer("Total", NULL);
	totalTimer->start();

	// Create the circuit
	Encoding* encoding = new Encoding();
	Field* field = encoding->getSrcField();
	BoolCircuit circuit(config["file"].as<string>());

	// Assign random inputs
	for (BoolWireVector::iterator iter = circuit.inputs.begin();
	     iter != circuit.inputs.end();
			 iter++) {
		(*iter)->value = rand() % 2 == 0;		
	}

	// Time the basic circuit evaluation
	TIME(circuit.eval(), "StdEval", "Total");
	
	// Construct the QAP
	Timer* constructionTime = timers.newTimer("Construct", "Total");
	constructionTime->start();
	QSP qsp(&circuit, encoding);
	constructionTime->stop();	

	Keys* keys = NULL;
	TIME(keys = qsp.genKeys(config), "KeyGen", "Total");

	QspProof* p = NULL;
	TIME(p = qsp.doWork(keys->pk, config), "DoWork", "Total");

	bool success;
	TIME(success = qsp.verify(keys, p), "Verify", "Total");

	assert(success);
	totalTimer->stop();

	printf("\n");
	keys->printSizes(config.count("raw"));
	if (config.count("raw")) {
		timers.printRaw();
	} else {
		timers.printStats();
	}

	delete encoding;
	delete keys;
	delete p;
	
}

/** Dispatcher **/
void runQspTests(variables_map& config) {
	if (config.count("file")) {
		runQspPerfTest(config);
	} else {
		runSimpleQspTests(config);
	}	
}

#else // !USE_OLD_PROTO

void runQspTests(variables_map& config) {
	printf("Enable USE_OLD_PROTO to run QSP-based tests\n");
}

#endif // USE_OLD_PROTO