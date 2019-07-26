#ifndef _TIMER_H
#define _TIMER_H

// Hunting for memory leaks
#ifdef _DEBUG
#define CRTDBG_MAP_ALLOC
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
// Print file names for the locations of memory leaks
#define DEBUG_NEW new(_NORMAL_BLOCK ,__FILE__, __LINE__)
#define new DEBUG_NEW
#endif
// Done with code for hunting memory leaks


#ifdef _WIN32
#define _WINSOCKAPI_    // stops windows.h including winsock.h
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif // _WIN32

#include <list>
#include <map>
#include <string>

using namespace::std;

typedef unsigned long long uint64_t;


class Timer { 
public: 
	Timer(void);
	Timer(const char* name, const char* parent);

	void start();
	void stop();
	void reset();

	double getTimeSeconds();
	uint64_t getTimeCycles();

	const char* getName() { return name; }
	const char* getParent() { return parent; }

	void print();
	void printRaw();
private:
	uint64_t _start;
	uint64_t _stop;
	uint64_t total;

	int id;
	const char* name;
	const char* parent;

	static double cpuFreq;
};

/*
struct ltstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }
};

// Node for a tree of timer data
// Leaf nodes are actual timers (timer != NULL)
// Subtrees represent the constituent parts of an operation (timer == NULL) 
typedef struct  {
	Timer* timer;
	std::map<const char*, TimerTree*, ltstr > children;
} TimerTree;
*/

typedef std::list<Timer*> ListOfTimers;
typedef std::map<string,Timer*> TimerMap;
typedef std::multimap<string,Timer*> TimerParentMap;
typedef std::pair<string,Timer*> NamedTimer;

class TimerList {
public:
	TimerList();
	~TimerList();

	void cleanup();

	void printRaw();	
	void printStats();

	Timer* newTimer(const char* name, const char* parent);
	Timer* newTimer(const char* name, const char* parent, bool addToTimerHierarchy);

	// Total time (in seconds) for all the timers in this list
	double totalTimeSeconds();

	static int newTimerId();
private:
	ListOfTimers timers;
	static int timerIDctr;

	// Helper function
	void printStats(TimerParentMap& parents, TimerMap& timerMap, string curNode, double parentTime, int depth);
};

// Shortcut for timing a single operation
#define TIME(op, name, parent) { \
	Timer* t = timers.newTimer(name, parent);	\
	t->start();	\
	op;	\
	t->stop();	\
}

// Shortcut for timing an operation repeated numTimes.  Use i as the index variable.
#define TIMEREP(op, numTimes, name, parent) { \
	Timer* t = timers.newTimer(name, parent);	\
	t->start();	\
	for (int i = 0; i < numTimes; i++) { \
		op;	\
	} \
	t->stop();	\
}


#endif // _TIMER_H