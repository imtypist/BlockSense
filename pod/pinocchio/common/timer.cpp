#define _INTSAFE_H_INCLUDED_
#include <cstdint>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "timer.h"

#ifdef _WIN32
#include <intrin.h>
#include <Windows.h>
#pragma intrinsic(__rdtsc)
#else 
#include <unistd.h> /* For sleep() */
#define Sleep(x) usleep(x*1000)
#endif 

double cpuFreq = 0;
double Timer::cpuFreq = 0;
int TimerList::timerIDctr = 1;

uint64_t GetRDTSC() {
	//VS on x64 doesn't support inline assembly
   //__asm {
   //   ; Flush the pipeline
   //   XOR eax, eax
   //   CPUID
   //   ; Get RDTSC counter in edx:eax
   //   RDTSC
   //}
#ifdef _WIN32
	int CPUInfo[4];
	int InfoType = 0;
   __cpuid(CPUInfo, InfoType); // Serialize the pipeline
   return (unsigned __int64)__rdtsc();
#else
	uint64_t rv;
	__asm__ (
		"push		%%ebx;"
		"cpuid;"
		"pop		%%ebx;"
		:::"%eax","%ecx","%edx");
	__asm__ __volatile__ ("rdtsc" : "=A" (rv));
	return (rv);
#endif // _WIN32
}

/*
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
*/

Timer::Timer() {
	total = 0;
}

Timer::Timer(const char* name, const char* parent) {
	this->name = name;
	this->parent = parent;
	this->total = 0;
	this->id = TimerList::newTimerId();
}


void Timer::start() {
	_start = GetRDTSC();
}

void Timer::stop() {
	_stop = GetRDTSC();
	total += _stop - _start;
}

void Timer::reset() {
	total = 0;
}

double Timer::getTimeSeconds() {
	if (Timer::cpuFreq == 0) {
		// Calibrate frequency
		uint64_t startTime = GetRDTSC();
		Sleep(200);  // Sleep for 200ms
		uint64_t stopTime = GetRDTSC();
	
		Timer::cpuFreq = (stopTime - startTime) / (200 / 1000.0);

		//cout << "Measured CPU at: " << cpuFreq << endl;
		printf("Measured CPU at %f GHz\n", Timer::cpuFreq/pow(10.0, 9));
		assert(Timer::cpuFreq);
	}

	return total / cpuFreq;
}

uint64_t Timer::getTimeCycles() {
	return total;
}

void Timer::print() {
	printf("%s:\t%0.3fs", name, getTimeSeconds());
}


void Timer::printRaw() {
	printf("%sSecs: %f %I64u", name, getTimeSeconds(), getTimeCycles());
}

TimerList::TimerList() {

}

Timer* TimerList::newTimer(const char* name, const char* parent) {
	Timer* timer = new Timer(name, parent);
	timers.push_back(timer);
	return timer;
}

Timer* TimerList::newTimer(const char* name, const char* parent, bool addToTimerHierarchy) {
	Timer* timer = new Timer(name, parent);
	if (addToTimerHierarchy) {
		timers.push_back(timer);
	}	
	return timer;
}

void TimerList::cleanup() {
	for (ListOfTimers::iterator iter = timers.begin();
		 iter != timers.end();
		 iter++) {
		delete *iter;		
	}	
	timers.clear();
}


TimerList::~TimerList() {
	cleanup();
}

int TimerList::newTimerId() {
	return timerIDctr++;
}

void TimerList::printRaw() {
	for (ListOfTimers::iterator iter = timers.begin();
		 iter != timers.end();
		 iter++) {
		(*iter)->printRaw();
		printf("\n");
	}	
}

bool parentLessThan(Timer* t1, Timer* t2) {
	// Break ties based on timer name
	if (t1->getParent() == NULL && t2->getParent() == NULL) {		
		return strcmp(t1->getName(), t2->getName()) < 0;
	}

	// Otherwise, NULL beats non-NULL
	if (t1->getParent() == NULL) { return true; }
	if (t2->getParent() == NULL) { return false; }

	// Else, decide based on parent
	return strcmp(t1->getParent(), t2->getParent()) < 0;
}

bool strEq(const char* s1, const char* s2) {
	if (s1 == NULL && s2 == NULL) {
		return true;
	}

	if ( (s1 == NULL && s2 != NULL) ||
		 (s1 != NULL && s2 == NULL)) {
		return false;
	}

	return strcmp(s1, s2) == 0;
}

void printGroup(ListOfTimers masterList, ListOfTimers tmpList) {
	Timer* head = tmpList.front();
	Timer* parent = NULL;
	double totalTime = 0;

	if (head->getParent() == NULL) {
		return;
		/*
		for (ListOfTimers::iterator iter = tmpList.begin();
			iter != tmpList.end();
			iter++) {
			totalTime += (*iter)->getTimeSeconds();
		}		
		*/
	} else {
		// Find the parent
		for (ListOfTimers::iterator iter = masterList.begin();
			 iter != masterList.end();
			 iter++) {
			if (strEq((*iter)->getName(), head->getParent())) {
				parent = (*iter);
				break;
			}
		}

		parent->print();
		printf("\n");
		totalTime = parent->getTimeSeconds();
	}
	
	for (ListOfTimers::iterator iter = tmpList.begin();
		iter != tmpList.end();
		iter++) {
		printf("\t");
		(*iter)->print();
		printf("\t(%0.1f%%)\n", (*iter)->getTimeSeconds() / totalTime * 100);
	}
}

void printTabs(int num) {
	for (int i = 0; i < num; i++) {
		printf("\t");
	}
}

void TimerList::printStats(TimerParentMap& parents, TimerMap& timerMap, string curNode, double parentTime, int depth) {
	TimerMap::iterator item = timerMap.find(curNode);
	double myTime = 0;

	if (item != timerMap.end()) {
		// Print info about this guy: name time %time
		printTabs(depth);
		item->second->print();
		myTime = item->second->getTimeSeconds();
		printf("\t(%0.1f%%)", myTime / parentTime * 100);
	}

	// Collect stats from the children (if any)
	double accountedForTime = 0;
	pair<TimerParentMap::iterator, TimerParentMap::iterator> range = parents.equal_range(curNode);
	for (TimerParentMap::iterator iter = range.first; iter != range.second; iter++) {
		accountedForTime += (*iter).second->getTimeSeconds();
	}

	if (myTime != 0 && accountedForTime != 0) {
		printf("\t%0.3fs\t(%0.1f%%) accounted for\n", accountedForTime, accountedForTime / myTime * 100);
	} else {
		printf("\n");
	}

	if (myTime == 0) { // Don't want the recursion to divide by 0!
		myTime = accountedForTime;
	}
		
	// Now recurse through the children (if any)
	for (TimerParentMap::iterator iter = range.first; iter != range.second; iter++) {
		printStats(parents, timerMap, (*iter).second->getName(), myTime, depth+1);
	}
}

void TimerList::printStats() {
	// Organize the timers based on parent, plus create a faster map for the timers themselves
	TimerParentMap parents;
	TimerMap timerMap;
	for (ListOfTimers::iterator iter = timers.begin();
		 iter != timers.end();
		 iter++) {		
		if ((*iter)->getParent() == NULL) {
			parents.insert(NamedTimer(string(), (*iter)));
		} else {
			parents.insert(NamedTimer((*iter)->getParent(), (*iter)));
		}
		timerMap.insert(NamedTimer((*iter)->getName(), (*iter)));
	}

	// Recursively print stats for each parent's children, starting at the root (NULL)
	printStats(parents, timerMap, "", 0, -1);

#if 0
	timers.sort(parentLessThan);
	const char* prevParent = NULL;

	ListOfTimers tmpList;

	for (ListOfTimers::iterator iter = timers.begin();
		 iter != timers.end();
		 iter++) {
		if (strEq((*iter)->getParent(), prevParent)) {
			// Add to the current group
			tmpList.push_back((*iter));
		} else {
			// Print the current group, dump it, and start anew
			if (!tmpList.empty()) {
				printGroup(timers, tmpList);
			}

			while (!tmpList.empty()) { tmpList.pop_front(); }

			tmpList.push_back((*iter));
			prevParent = (*iter)->getParent();
		}
	}

	// Print the final group
	printGroup(timers, tmpList);
#endif
}


double TimerList::totalTimeSeconds() {
	double total = 0;
	for (ListOfTimers::iterator iter = timers.begin();
		 iter != timers.end();
		 iter++) {
		total += (*iter)->getTimeSeconds(); 		
	}	
	return total;
}