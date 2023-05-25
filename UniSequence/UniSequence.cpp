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
		syncThreadFlag = true;
		httplib::Client requestClient(SERVER_ADDRESS_PRC);
		requestClient.set_connection_timeout(5, 0);
		while (true)
		{
			for (auto& airport : airportList)
			{
				if (auto result = requestClient.Get(SERVER_RESTFUL_VER + airport + "/queue"))
				{
					json resObj = json::parse(result->body);
					int seqNumber = 1;
					for (auto& seqObj : resObj["data"])
					{
						SyncSeq(seqObj["callsign"], seqObj["status"]);
						SyncSeqNum(seqObj["callsign"], seqNumber);
						seqNumber++;
					}
				}
				else
				{
					Messager(httplib::to_string(result.error()));
				}
			}
			this_thread::sleep_for(chrono::seconds(timerInterval));
		}
		});
	logonCode = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
	if (!logonCode)
	{
		Messager("You haven't set your logon code, please set your code first before using this plugin by enter \".sqc <your code here>\".");
	}
	else
	{
		dataSyncThread->detach();
	}
	if (syncThreadFlag) Messager("Data synchronize thread detached.");
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
	if (cmd.substr(0, 4) == ".SQC")
	{
		try
		{
			stringstream ss(cmd);
			char delim = ' ';
			string item;
			airportList.clear();
			while (getline(ss, item, delim))
			{
				if (!item.empty() && item[0] != '.') SaveDataToSettings(PLUGIN_SETTING_KEY_LOGON_CODE, PLUGIN_SETTING_DESC_LOGON_CODE, item.c_str()); // the command itself has been skiped here
			}
			Messager("Your logon code has been saved.");
		}
		catch (runtime_error const& e)
		{
			Messager("Error: " + *e.what());
		}
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
	if (!logonCode)
	{
		Messager(ERR_LOGON_CODE_NULLREF);
		return;
	}
	thread patchThread([fp, status, this] {
		json reqBody = {
			{JSON_KEY_CALLSIGN, fp.GetCallsign()},
			{JSON_KEY_STATUS, status},
			{JSON_KEY_LOGON_CODE, logonCode}
		};
		httplib::Client patchReq(SERVER_ADDRESS_PRC);
		patchReq.set_connection_timeout(3, 0);
		string ap = fp.GetFlightPlanData().GetOrigin();
		if (auto result = patchReq.Patch(SERVER_RESTFUL_VER + ap + "/status", reqBody.dump().c_str(), "application/json"))
		{
			if (result->status == 200)
			{
				SyncSeq(fp.GetCallsign(), status);
				json resObj = json::parse(result->body);
				int seqNumber = 1;
				for (auto& seqObj : resObj["data"])
				{
					SyncSeq(seqObj["callsign"], seqObj["status"]);
					SyncSeqNum(seqObj["callsign"], seqNumber);
					seqNumber++;
				}
			}
		}
		else
		{
			Messager(ERR_CONN_FAILED);
		}
		});
	patchThread.detach();
}

void UniSequence::RemoveFromSeq(CFlightPlan fp)
{
	int offset = 0;
	for (auto& ac : sequence)
	{
		if (ac.fp.GetCallsign() == fp.GetCallsign())
		{
			sequence.erase(sequence.begin() + offset);
			return;
		}
		offset++;
	}
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
		OpenPopupList(area, STATUS_LIST_TITLE, 2);
		AddPopupListElement(STATUS_DESC_WFCR, "", FUNC_SWITCH_TO_WFCR);
		AddPopupListElement(STATUS_DESC_CLRD, "", FUNC_SWITCH_TO_CLRD);
		AddPopupListElement(STATUS_DESC_WFPU, "", FUNC_SWITCH_TO_WFPU);
		AddPopupListElement(STATUS_DESC_PUSH, "", FUNC_SWITCH_TO_PUSH);
		AddPopupListElement(STATUS_DESC_WFTX, "", FUNC_SWITCH_TO_WFTX);
		AddPopupListElement(STATUS_DESC_TAXI, "", FUNC_SWITCH_TO_TAXI);
		AddPopupListElement(STATUS_DESC_WFTO, "", FUNC_SWITCH_TO_WFTO);
		AddPopupListElement(STATUS_DESC_TKOF, "", FUNC_SWITCH_TO_TOGA);
		break;
	case SEQUENCE_TAGITEM_FUNC_REORDER_EDITED:
		if (!logonCode)
		{
			Messager(ERR_LOGON_CODE_NULLREF);
			return;
		}
		reOrderThread = new thread([sItemString, fp, this] {
			httplib::Client req(SERVER_ADDRESS_PRC);
			string ap = fp.GetFlightPlanData().GetOrigin();
			json reqBody = {
				{JSON_KEY_CALLSIGN, fp.GetCallsign()},
				{JSON_KEY_BEFORE, sItemString},
				{JSON_KEY_LOGON_CODE, logonCode}
			};
			if (auto res = req.Patch(SERVER_RESTFUL_VER + ap + "/order", reqBody.dump(), "application/json"))
			{
				if (res->status == 200)
				{
					json resObj = json::parse(res->body);
					int seqNumber = 1;
					for (auto& seqObj : resObj["data"])
					{
						SyncSeq(seqObj["callsign"], seqObj["status"]);
						SyncSeqNum(seqObj["callsign"], seqNumber);
						seqNumber++;
					}
				}
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
	// remove aircraft if it's taking off
	if (rt.GetGS() > 50)
	{
		RemoveFromSeq(fp);
		return;
	}
	// automatically check if the departure airport of this fp is in the airport list
	string depAirport = fp.GetFlightPlanData().GetOrigin();
	CheckApEnabled(depAirport);
	SeqN* node = GetSeqN(fp);
	if (!node) PushToSeq(fp); // if is nullptr, push an new object to sequence
	SeqN* ac = GetSeqN(fp);
	int status = ac->status; // recapture the fp we just added
	int seqNum = ac->sequenceNumber;
	// you won't want to say "Your sequence number is one hundred and fivty nine" :D
	if (ac->sequenceNumber > 99) seqNum = 99;
	int bufferSize = strlen(STATUS_TEXT_PLACE_HOLDER) + 1;
	switch (status)
	{
	case AIRCRAFT_STATUS_NULL:
		sprintf_s(sItemString, bufferSize, "%s", STATUS_TEXT_NULL);
		break;
	case AIRCRAFT_STATUS_WFCR:
		sprintf_s(sItemString, bufferSize, STATUS_TEXT_FORMAT_STRING, seqNum, STATUS_TEXT_WFCR);
		*pColorCode = TAG_COLOR_RGB_DEFINED;
		*pRGB = STATUS_COLOR_WAIT;
		break;
	case AIRCRAFT_STATUS_CLRD:
		sprintf_s(sItemString, bufferSize, STATUS_TEXT_FORMAT_STRING, seqNum, STATUS_TEXT_CLRD);
		*pColorCode = TAG_COLOR_RGB_DEFINED;
		*pRGB = STATUS_COLOR_IN_PROGRESS;
		break;
	case AIRCRAFT_STATUS_WFPU:
		sprintf_s(sItemString, bufferSize, STATUS_TEXT_FORMAT_STRING, seqNum, STATUS_TEXT_WFPU);
		*pColorCode = TAG_COLOR_RGB_DEFINED;
		*pRGB = STATUS_COLOR_WAIT;
		break;
	case AIRCRAFT_STATUS_PUSH:
		sprintf_s(sItemString, bufferSize, STATUS_TEXT_FORMAT_STRING, seqNum, STATUS_TEXT_PUSH);
		*pColorCode = TAG_COLOR_RGB_DEFINED;
		*pRGB = STATUS_COLOR_IN_PROGRESS;
		break;
	case AIRCRAFT_STATUS_WFTX:
		sprintf_s(sItemString, bufferSize, STATUS_TEXT_FORMAT_STRING, seqNum, STATUS_TEXT_WFTX);
		*pColorCode = TAG_COLOR_RGB_DEFINED;
		*pRGB = STATUS_COLOR_WAIT;
		break;
	case AIRCRAFT_STATUS_TAXI:
		sprintf_s(sItemString, bufferSize, STATUS_TEXT_FORMAT_STRING, seqNum, STATUS_TEXT_TAXI);
		*pColorCode = TAG_COLOR_RGB_DEFINED;
		*pRGB = STATUS_COLOR_IN_PROGRESS;
		break;
	case AIRCRAFT_STATUS_WFTO:
		sprintf_s(sItemString, bufferSize, STATUS_TEXT_FORMAT_STRING, seqNum, STATUS_TEXT_WFTO);
		*pColorCode = TAG_COLOR_RGB_DEFINED;
		*pRGB = STATUS_COLOR_WAIT;
		break;
	case AIRCRAFT_STATUS_TOGA:
		sprintf_s(sItemString, bufferSize, STATUS_TEXT_FORMAT_STRING, seqNum, STATUS_TEXT_TOGA);
		*pColorCode = TAG_COLOR_RGB_DEFINED;
		*pRGB = STATUS_COLOR_IN_PROGRESS;
		break;
	}
	return;
}