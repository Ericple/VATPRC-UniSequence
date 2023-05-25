#pragma once

using namespace std;
using namespace EuroScopePlugIn;

#ifndef COPYRIGHTS
#define AUTHOR "Ericple Garrison"
#define GITHUB_LINK "https://github.com/Ericple/VATPRC-UniSequence"
#define DIVISION "VATPRC"
#define PLUGIN_NAME "UniSequence"
#define PLUGIN_VER "0.2.0-beta"
#define PLUGIN_AUTHOR "Ericple Garrison"
#define PLUGIN_COPYRIGHT "AGPL-3.0 license"
#endif

#ifndef Code
#define SEQUENCE_TAGITEM_TYPE_CODE 35
#define SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE 50
#define SEQUENCE_TAGITEM_FUNC_REORDER 82
#define SEQUENCE_TAGITEM_FUNC_REORDER_EDITED 94
#define FUNC_SWITCH_TO_WFCR 84
#define FUNC_SWITCH_TO_CLRD 86
#define FUNC_SWITCH_TO_WFPU 93
#define FUNC_SWITCH_TO_PUSH 99
#define FUNC_SWITCH_TO_WFTX 102
#define FUNC_SWITCH_TO_TAXI 106
#define FUNC_SWITCH_TO_WFTO 110
#define FUNC_SWITCH_TO_TOGA 114
#endif // !Code

#ifndef LIMITATIONS
#define MAXIMUM_AIRPORT_LIST_COUNT 100
#endif // !LIMITATIONS

#ifndef AIRCRAFT_STATUS
#define STATUS_TEXT_PLACE_HOLDER "________"
#define AIRCRAFT_STATUS_NULL 999 // EMPTY STATUS
#define STATUS_TEXT_NULL "-------"
#define AIRCRAFT_STATUS_WFCR 70 // WAITING FOR CLEARANCE
#define STATUS_TEXT_WFCR "-CLRD"
#define AIRCRAFT_STATUS_CLRD 60 // CLEARANCE GOT
#define STATUS_TEXT_CLRD "+CLRD"
#define AIRCRAFT_STATUS_WFPU 50 // WATING FOR PUSH
#define STATUS_TEXT_WFPU "-PUSH"
#define AIRCRAFT_STATUS_PUSH 40 // PUSHING BACK
#define STATUS_TEXT_PUSH "+PUSH"
#define AIRCRAFT_STATUS_WFTX 30 // WATING FOR TAXI
#define STATUS_TEXT_WFTX "-TAXI"
#define AIRCRAFT_STATUS_TAXI 20 // TAXI TO RWY
#define STATUS_TEXT_TAXI "+TAXI"
#define AIRCRAFT_STATUS_WFTO 10 // WAITING FOR TAKE OFF
#define STATUS_TEXT_WFTO "-TKOF"
#define AIRCRAFT_STATUS_TOGA 0 // TAKE OFF
#define STATUS_TEXT_TOGA "+TKOF"
#define STATUS_COLOR_WAIT RGB(255, 255, 0)
#define STATUS_COLOR_IN_PROGRESS RGB(0, 204,0)
#define STATUS_TEXT_FORMAT_STRING "%02d%s"
#endif // !AIRCRAFT_STATUS

typedef struct SequenceNode {
	EuroScopePlugIn::CFlightPlan fp;
	int status, sequenceNumber;
} SeqN;

class UniSequence : public CPlugIn
{
public:
	UniSequence();
	~UniSequence();
	future<string> bufferQueueString;
	virtual bool OnCompileCommand(const char*);
	virtual void OnFunctionCall(int, const char*, POINT, RECT);
	virtual void OnGetTagItem(CFlightPlan, CRadarTarget, 
		int, int, char[16], int*, COLORREF*, double*);
	void Messager(string);
	vector<string> airportList;
	vector<SeqN> sequence;
private:
	thread* dataSyncThread;
	int timerInterval = 15;
	SeqN* GetSeqN(CFlightPlan);
	void PushToSeq(CFlightPlan);
	void RemoveFromSeq(CFlightPlan);
	void CheckApEnabled(string);
	void SyncSeq(string, int);
	void SyncSeqNum(string, int);
	void PatchStatus(CFlightPlan, int);
};

