#include "pch.h"
#include "SequenceWorks.h"

#ifndef COPYRIGHTS
#define PLUGIN_NAME "UniSequence"
#define PLUGIN_VER "1.0.0"
#define PLUGIN_AUTHOR "Ericple Garrison"
#define PLUGIN_COPYRIGHT "GPL v3"
#endif // !COPYRIGHTS

using namespace std;

SequenceWorks::SequenceWorks(void) : CPlugIn(
	EuroScopePlugIn::COMPATIBILITY_CODE,
	PLUGIN_NAME,PLUGIN_VER,PLUGIN_AUTHOR,PLUGIN_COPYRIGHT
)
{
}

void SequenceWorks::Messager(string msg)
{
	DisplayUserMessage("Sequence Works", "SeqWorker", msg.c_str(),
		true, true, true, true, true);
}

bool SequenceWorks::UpdateSeq(CFlightPlan fp, int status)
{
	for (auto& seqNode : Sequence)
	{
		if (seqNode.fp.GetCallsign() == fp.GetCallsign())
		{
			nlohmann::json requestBody = {
				{"callsign", fp.GetCallsign()},
				{"status", status},
			};
			httplib::Client client(SERVER_ADDRESS_PRC, SERVER_PORT_PRC);
			client.set_connection_timeout(5, 0);
			client.Patch(SERVER_RESTFUL_VER + *fp.GetFlightPlanData().GetOrigin() + *"/status", requestBody.dump(), "application/json");
			seqNode.status = status;
			return true;
		}
	}
	Messager("Err: Attempting to update an unexist element.");
	return false;
}
bool SequenceWorks::AddToSeq(CFlightPlan fp)
{
	try
	{
		for (auto& seqNode : Sequence)
		{
			if (seqNode.fp.GetCallsign() == fp.GetCallsign())
			{
				Messager("Err: Callsign confict when attempting to add " + *fp.GetCallsign() + *"to sequence");
				return false;
			}
		}
		SeqN newEl = {
		fp,
		AIRCRAFT_STATUS_NULL
		};
		Sequence.push_back(newEl);
		return true;
	}
	catch (exception const& e)
	{
		Messager(e.what());
		return false;
	}
}

SequenceWorks::~SequenceWorks(void){}
