#include "pch.h"
#include "UniSequence.h"
#include "queueItem.h"

using namespace std;
using nlohmann::json;

void UniSequence::SyncSeq(string callsign, int status)
{
	for (auto& seqN : sequence)
	{
		if (seqN.fp.GetCallsign() == callsign)
		{
			seqN.status = status;
			return;
		}
	}
}

void UniSequence::SyncSeqNum(string callsign, int seqNum)
{
	for (auto& seqN : sequence)
	{
		if (seqN.fp.GetCallsign() == callsign)
		{
			seqN.sequenceNumber = seqNum;
			return;
		}
	}
}

UniSequence::UniSequence(void) : CPlugIn(
	EuroScopePlugIn::COMPATIBILITY_CODE,
	PLUGIN_NAME, PLUGIN_VER, PLUGIN_AUTHOR, PLUGIN_COPYRIGHT
)
{
	RegisterTagItemType("Sequence", SEQUENCE_TAGITEM_TYPE_CODE);
	RegisterTagItemFunction("Sequence Popup List", SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE);
	RegisterTagItemFunction("Sequence Reorder", SEQUENCE_TAGITEM_FUNC_REORDER);
	dataSyncThread = new thread([&] {
		httplib::Client requestClient(SERVER_ADDRESS_PRC, SERVER_PORT_PRC);
		requestClient.set_connection_timeout(5, 0);
		while (true)
		{
			for (auto& airport : airportList)
			{
				if (auto result = requestClient.Get(SERVER_RESTFUL_VER + airport + "/queue"))
				{
					Messager("Start sync.");
					json resObj = json::parse(result->body);
					int seqNumber = 1;
					for (auto& seqObj : resObj["data"])
					{
						SyncSeq(seqObj["callsign"], seqObj["status"]);
						SyncSeqNum(seqObj["callsign"], seqNumber);
						seqNumber++;
					}
					Messager("Sync complete.");
				}
				else
				{
					Messager(httplib::to_string(result.error()));
				}
			}
			this_thread::sleep_for(chrono::seconds(timerInterval));
		}
		});
	dataSyncThread->detach();
	Messager("UniSequence Plugin Loaded.");
}

UniSequence::~UniSequence(void){}

void UniSequence::Messager(string message)
{
	DisplayUserMessage("UniSequence", "system", message.c_str(),
		false, true, true, false, true);
}

SeqN* UniSequence::GetSeqN(CFlightPlan fp)
{
	for (auto& node : sequence)
	{
		if (node.fp.GetCallsign() == fp.GetCallsign()) return &node;
	}
	return (SeqN*)nullptr;
}

bool UniSequence::OnCompileCommand(const char* sCommandLine)
{
	string cmd = sCommandLine;

	// transform cmd to uppercase in order for identify
	transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

	// display copyright and help link information
	if (cmd.substr(0, 5) == ".UNIS")
	{
		Messager("UniSequence Plugin For VATPRC DIVISION.");
		Messager("Author: Ericple Garrison");
		Messager("For help and bug report, refer to https://github.com/Ericple/VATPRC-UniSequence");
		return true;
	}
	// Set airports to be listened
	if (cmd.substr(0, 4) == ".SQA")
	{
		try
		{
			stringstream ss(cmd);
			char delim = ' ';
			string item;
			airportList.clear();
			while (getline(ss, item, delim))
			{
				if (!item.empty() && item[0] != '.') airportList.push_back(item); // the command itself has been skiped here
			}
			if (airportList.size() > 1) Messager("Airports saved.");
			if (airportList.size() == 1) Messager("Airport saved.");
			if (airportList.size() < 1) Messager("Airport cleared.");
		}
		catch (runtime_error const& e)
		{
			Messager("Error: " + *e.what());
		}
		return true;
	}
	if (cmd.substr(0, 4) == ".SQP")
	{
		char i[10];
		_itoa_s(airportList.size(), i, 10);
		Messager("Current in list: ");
		Messager(i);
		return true;
	}
	if (cmd.substr(0, 4) == ".SQS")
	{
		char i[10];
		_itoa_s(sequence.size(), i, 10);
		Messager("Current in sequence:");
		Messager(i);
		return true;
	}

	return false;
}

void UniSequence::PushToSeq(CFlightPlan fp)
{
	SeqN seqN = *new SeqN{ fp, AIRCRAFT_STATUS_NULL };
	sequence.push_back(seqN);
}

void UniSequence::PatchStatus(CFlightPlan fp, int status)
{
	thread patchThread([fp, status, this] {
		json reqBody = {
				{"callsign", fp.GetCallsign()},
				{"status", status},
		};
		httplib::Client patchReq(SERVER_ADDRESS_PRC, SERVER_PORT_PRC);
		patchReq.set_connection_timeout(3, 0);
		string ap = fp.GetFlightPlanData().GetOrigin();
		if (auto result = patchReq.Patch(SERVER_RESTFUL_VER + ap + "/status", reqBody.dump().c_str(), "application/json"))
		{
			if (result->status == 200)
			{
				SyncSeq(fp.GetCallsign(), status);
				Messager("Status updated");
			}
		}
		else
		{
			Messager("Failed to connect to queue server.");
		}
		});
	patchThread.detach();
}

void UniSequence::OnFunctionCall(int fId, const char* sItemString, POINT pt, RECT area)
{
	CFlightPlan fp;
	fp = FlightPlanSelectASEL();
	if (!fp.IsValid()) return;
	thread* reOrderThread;
	switch (fId)
	{
	case SEQUENCE_TAGITEM_FUNC_REORDER:
		OpenPopupEdit(area, SEQUENCE_TAGITEM_FUNC_REORDER_EDITED, "");
		break;
	case SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE:
		OpenPopupList(area, "Status", 2);
		AddPopupListElement("Waiting for clearance", "", FUNC_SWITCH_TO_WFCR);
		AddPopupListElement("Clearance granted", "", FUNC_SWITCH_TO_CLRD);
		AddPopupListElement("Waiting for push", "", FUNC_SWITCH_TO_WFPU);
		AddPopupListElement("Pushing", "", FUNC_SWITCH_TO_PUSH);
		AddPopupListElement("Waiting for taxi", "", FUNC_SWITCH_TO_WFTX);
		AddPopupListElement("Taxiing", "", FUNC_SWITCH_TO_TAXI);
		AddPopupListElement("Waiting for take off", "", FUNC_SWITCH_TO_WFTO);
		AddPopupListElement("Taking Off / Go Around", "", FUNC_SWITCH_TO_TOGA);
		break;
	case SEQUENCE_TAGITEM_FUNC_REORDER_EDITED:
		Messager("Performing sequence reorder...");
		reOrderThread = new thread([sItemString, fp, this] {
			httplib::Client req(SERVER_ADDRESS_PRC, SERVER_PORT_PRC);
			string ap = fp.GetFlightPlanData().GetOrigin();
			json reqBody = {
				{"callsign", fp.GetCallsign()},
				{"before", sItemString}
			};
			if (auto res = req.Patch(SERVER_RESTFUL_VER + ap + "/order", reqBody.dump(), "application/json"))
			{
				Messager("Sequence order edited.");
			}
			else
			{
				Messager(httplib::to_string(res.error()));
			}
			});
		Messager(sItemString);
		break;
	case FUNC_SWITCH_TO_WFCR:
		PatchStatus(fp, AIRCRAFT_STATUS_WFCR);
		break;
	case FUNC_SWITCH_TO_CLRD:
		PatchStatus(fp, AIRCRAFT_STATUS_CLRD);
		break;
	case FUNC_SWITCH_TO_WFPU:
		PatchStatus(fp, AIRCRAFT_STATUS_WFPU);
		break;
	case FUNC_SWITCH_TO_PUSH:
		PatchStatus(fp, AIRCRAFT_STATUS_PUSH);
		break;
	case FUNC_SWITCH_TO_WFTX:
		PatchStatus(fp, AIRCRAFT_STATUS_WFTX);
		break;
	case FUNC_SWITCH_TO_TAXI:
		PatchStatus(fp, AIRCRAFT_STATUS_TAXI);
		break;
	case FUNC_SWITCH_TO_WFTO:
		PatchStatus(fp, AIRCRAFT_STATUS_WFTO);
		break;
	case FUNC_SWITCH_TO_TOGA:
		PatchStatus(fp, AIRCRAFT_STATUS_TOGA);
	default:
		break;
	}
}

void UniSequence::CheckApEnabled(string depAirport)
{
	for (auto& airport : airportList)
	{
		if (airport == depAirport)
		{
			return;
		}
	}
	airportList.push_back(depAirport);
}

void UniSequence::OnGetTagItem(CFlightPlan fp, CRadarTarget rt, int itemCode, int tagData,
	char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize)
{
	// check if this tag is valid
	if (!fp.IsValid() || !rt.IsValid()) return;
	// check if item code is what we need to handle
	if (itemCode != SEQUENCE_TAGITEM_TYPE_CODE) return;
	// automatically check if the departure airport of this fp is in the airport list
	string depAirport = fp.GetFlightPlanData().GetOrigin();
	CheckApEnabled(depAirport);
	SeqN* node = GetSeqN(fp);
	if (!node) PushToSeq(fp); // if is nullptr, push an new object to sequence
	SeqN* ac = GetSeqN(fp);
	int status = ac->status; // recapture the fp we just added
	int seqNum = ac->sequenceNumber;
	int bufferSize = strlen(STATUS_TEXT_PLACE_HOLDER) + 1;
	switch (status)
	{
	case AIRCRAFT_STATUS_NULL:
		sprintf_s(sItemString, bufferSize, "%s", STATUS_TEXT_NULL);
		break;
	case AIRCRAFT_STATUS_WFCR:
		sprintf_s(sItemString, bufferSize, "%02d%s", seqNum, STATUS_TEXT_WFCR);
		break;
	case AIRCRAFT_STATUS_CLRD:
		sprintf_s(sItemString, bufferSize, "%02d%s", seqNum, STATUS_TEXT_CLRD);
		break;
	case AIRCRAFT_STATUS_WFPU:
		sprintf_s(sItemString, bufferSize, "%02d%s", seqNum, STATUS_TEXT_WFPU);
		break;
	case AIRCRAFT_STATUS_PUSH:
		sprintf_s(sItemString, bufferSize, "%02d%s", seqNum, STATUS_TEXT_PUSH);
		break;
	case AIRCRAFT_STATUS_WFTX:
		sprintf_s(sItemString, bufferSize, "%02d%s", seqNum, STATUS_TEXT_WFTX);
		break;
	case AIRCRAFT_STATUS_TAXI:
		sprintf_s(sItemString, bufferSize, "%02d%s", seqNum, STATUS_TEXT_TAXI);
		break;
	case AIRCRAFT_STATUS_WFTO:
		sprintf_s(sItemString, bufferSize, "%02d%s", seqNum, STATUS_TEXT_WFTO);
		break;
	case AIRCRAFT_STATUS_TOGA:
		sprintf_s(sItemString, bufferSize, "%02d%s", seqNum, STATUS_TEXT_TOGA);
		break;
	}
	return;
}