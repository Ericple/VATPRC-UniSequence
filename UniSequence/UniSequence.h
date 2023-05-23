#pragma once
#include "SequenceWorks.h"

using namespace std;

#ifndef COPYRIGHTS
#define AUTHOR "Ericple Garrison"
#define GITHUB_LINK "Closed source for now"
#define DIVISION "VATPRC"
#endif

#ifndef Code
#define SEQUENCE_TAGITEM_TYPE_CODE 1
#define SEQUENCE_TAGITEM_FUNC_CODE 1
#endif // !Code

#ifndef LIMITATIONS
#define MAXIMUM_AIRPORT_LIST_COUNT 100
#endif // !LIMITATIONS

class UniSequence : public SequenceWorks
{
public:
	UniSequence();
	~UniSequence();
	future<string> bufferQueueString;
	virtual bool OnCompileCommand(const char*);
	virtual void OnGetTagItem(EuroScopePlugIn::CFlightPlan, EuroScopePlugIn::CRadarTarget, 
		int, int, char[], int*, COLORREF*, double*);
	void Messager(string);
	virtual void OnTimer(int);
	vector<string> airportList;
	map<string, string> QueueCache;
private:
	string FetchQueue(string); // fetch queue data from network to target address
	void FetchQueueSocket();
	int timerInterval = 5;
};
