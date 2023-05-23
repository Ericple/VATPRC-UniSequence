#pragma once

#ifndef AIRCRAFT_STATUS
#define AIRCRAFT_STATUS_NULL 999 // EMPTY STATUS
#define AIRCRAFT_STATUS_WFCR 70 // WAITING FOR CLEARANCE
#define AIRCRAFT_STATUS_CLRD 60 // CLEARANCE GOT
#define AIRCRAFT_STATUS_WFPU 50 // WATING FOR PUSH
#define AIRCRAFT_STATUS_PUSH 40 // PUSHING BACK
#define AIRCRAFT_STATUS_WFTX 30 // WATING FOR TAXI
#define AIRCRAFT_STATUS_TAXI 20 // TAXI TO RWY
#define AIRCRAFT_STATUS_WFTO 10 // WAITING FOR TAKE OFF
#define AIRCRAFT_STATUS_TOGA 0 // TAKE OFF
#endif // !AIRCRAFT_STATUS

#ifndef LIST_LIMITATION
#define MAXIMUM_LIST_LENGTH 500
#define UNSET_LIST_CALLSIGN "UNSET_CALLSIGN"
#endif // !LIST_LIMITATION

using namespace EuroScopePlugIn;

typedef struct SequenceNode {
	CFlightPlan fp;
	int status;
} SeqN;


class SequenceWorks : public CPlugIn
{
public:
	SequenceWorks();
	~SequenceWorks();
	std::list<SeqN> Sequence;
	bool UpdateSeq(CFlightPlan, int);
	void SyncSeq(std::string, int);
	int GetSeq(std::string);
	bool AddToSeq(CFlightPlan);
	bool DeleteFromSeq(CFlightPlan);
	bool PatchRemoteStatus(CFlightPlan, int);
	// Patch order of remote server. Arg1 is callsign of aircraft to be moved, the other is the callsign of which will be placed before.
	bool PatchRemoteOrder(CFlightPlan, CFlightPlan);
private:
	void Messager(std::string);
};

