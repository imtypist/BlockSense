#pragma once

#define WIN32_LEAN_AND_MEAN  // Prevent conflicts in Windows.h and WinSock.h
#pragma warning( disable : 4005 )	// Ignore warning about intsafe.h macro redefinitions

// Uncomment to compress EC points before writing them to an archive
#define CONDENSE_ARCHIVES

#ifdef _DEBUG
// Uncomment this line to check for memory leaks using VS.NET
//#define MEM_LEAK

// Uncomment this line to use a small identity encoding field
//#define DEBUG_ENCODING

// Uncomment this line to use sequential, rather than random, field elements
//#define DEBUG_RANDOM

// Uncomment to skip the strengthening step.  Legit evals should still pass.
//#define DEBUG_STRENGTHEN

// Perform extra checks on elliptic curve operations
//#define DEBUG_EC

// Compare results between the PV and the DV case
//#define DEBUG_PV_VS_DV

//#define DEBUG_FAST_DV_IO

#define VERBOSE

#endif


// Revert to the old version of our QAP protocol (from the EuroCrypt paper)
//#define USE_OLD_PROTO 

#ifdef USE_OLD_PROTO
// If defined, pushes more of the key generation work off to the worker.
//#define PUSH_EXTRA_WORK_TO_WORKER
#ifdef PUSH_EXTRA_WORK_TO_WORKER
// Enable the option below to have the worker invest a large effort to precompute polynomials the verifier used to supply.  
// Experiments suggest it's unlikely to be a win
//#define PRE_COMP_POLYS 
#endif // PUSH_EXTRA_WORK_TO_WORKER
#endif // USE_OLD_PROTO

// Ensure consistency by checking all IO at polys that are non-zero on both V and W
#define CHECK_NON_ZERO_IO 1

// Use Enigma's multi-mul routine instead of our new, memory-intense version
//#define OLD_MULTI_EXP

// Hunting for memory leaks
#ifdef MEM_LEAK
#define CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
// Print file names for the locations of memory leaks
#define DEBUG_NEW new(_NORMAL_BLOCK ,__FILE__, __LINE__)
#define new DEBUG_NEW
#endif
// Done with code for hunting memory leaks


// Microsoft's bignum library expects NDEBUG to be defined (or badness occurs!)
#ifndef NDEBUG
  #define UNDEFINENDEBUG
  #define NDEBUG
#endif
#include <msbignum.h>
#include <field.h>
#include "pfc_bn.h"
#include "modfft.h"
#ifdef UNDEFINENDEBUG
  #undef NDEBUG
  #undef UNDEFINENDEBUG
#endif


#ifndef DEBUG_ENCODING
typedef digit_t FieldElt[4];
#else // DEBUG_ENCODING
typedef digit_t FieldElt[1];
#endif // DEBUG_ENCODING

// When we recurse through the circuit, 
// keep track of root modifiers that result 
// from constant addition or mult gates
typedef struct {
	FieldElt add;
	FieldElt mul;
} Modifier;

#include "timer.h"

extern TimerList timers;