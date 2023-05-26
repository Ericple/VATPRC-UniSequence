#include "pch.h"
#include "UniSequence.h"
#include "queueItem.h"

using namespace std;
using nlohmann::json;

void UniSequence::log(string message)
{
	SYSTEMTIME sysTime = { 0 };
	GetSystemTime(&sysTime);
	if (!logStream.is_open()) logStream.open(LOG_FILE_NAME, ios::app);
	logStream << "[" << sysTime.wHour << ":" << sysTime.wMinute << ":" << sysTime.wSecond << "] " << message << endl;
	logStream.close();
}

void UniSequence::endLog()
{
	log("End log function was called, function will now stop and plugin unloaded.");
	if(logStream.is_open()) logStream.close();
}

void UniSequence::SyncSeq(string callsign, int status)
{
	log("Synchronizing sequence status for " + callsign);
	for (auto& seqN : sequence)
	{
		if (seqN.fp.GetCallsign() == callsign)
		{
#ifdef ENV_DEBUG
			OutputDebugStringA("Synchronizing status of " + *callsign.c_str());
#endif // ENV_DEBUG
			seqN.status = status;
			log("Sync complete.");
			return;
		}
	}
}

void UniSequence::SyncSeqNum(string callsign, int seqNum)
{
	log("Synchronizing sequence number for " + callsign);
	for (auto& seqN : sequence)
	{
		if (seqN.fp.GetCallsign() == callsign)
		{
#ifdef ENV_DEBUG
			OutputDebugStringA("Synchronizing sequence number");
#endif // ENV_DEBUG
			seqN.sequenceNumber = seqNum;
			log("Sync complete.");
			return;
		}
	}
}

UniSequence::UniSequence(void) : CPlugIn(
	EuroScopePlugIn::COMPATIBILITY_CODE,
	PLUGIN_NAME, PLUGIN_VER, PLUGIN_AUTHOR, PLUGIN_COPYRIGHT
)
{
	if (!remove(LOG_FILE_NAME)) Messager("log file not found.");
	log("UniSequence initializing");
	log("Attempting to register tag item type of \"Sequence\"");
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
				log("Sync data of " + airport);
				if (auto result = requestClient.Get(SERVER_RESTFUL_VER + airport + "/queue"))
				{
					log("Data fetched, updating local sequence list");
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
#ifdef PATCH_WITH_LOGON_CODE
	log("Reading settings from EuroScope.");
	logonCode = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
	if (!logonCode)
	{
		log("Logon code not set.");
		Messager(ERR_LOGON_CODE_NULLREF);
		syncThreadFlag = false;
	}
	else
	{
		log("Start synchronizing sequence data with server.");
		dataSyncThread->detach();
		syncThreadFlag = true;
	}
	if (syncThreadFlag) Messager("Data synchronize thread detached.");
#else
	log("Start synchronizing sequence data with server.");
	dataSyncThread->detach();
#endif // PATCH_WITH_LOGON_CODE

	Messager("UniSequence Plugin Loaded.");
	log("Initialization complete.");
}

UniSequence::~UniSequence(void){}

void UniSequence::Messager(string message)
{
	DisplayUserMessage("UniSequence", "system", message.c_str(),
		false, true, true, true, true);
	log(message);
}

SeqN* UniSequence::GetSeqN(CFlightPlan fp)
{
	string cs = fp.GetCallsign();
	for (auto& node : sequence)
	{
		if (node.fp.GetCallsign() == fp.GetCallsign()) return &node;
	}
	log("There's no sequence node for " + cs + ", returning nullptr");
	return (SeqN*)nullptr;
}

bool UniSequence::OnCompileCommand(const char* sCommandLine)
{
	string cmd = sCommandLine;
	log("Command received: " + cmd);
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
		log("command \".SQA\" acknowledged.");
		try
		{
			log("Spliting cmd");
			stringstream ss(cmd);
			char delim = ' ';
			string item;
			log("Clearing airport list");
			airportList.clear();
			log("Airport list cleared");
			while (getline(ss, item, delim))
			{
				if (!item.empty() && item[0] != '.')  // the command itself has been skiped here
				{
					log("Adding " + item + " to airport list");
					airportList.push_back(item);
				}
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
		log("command \".SQP\" acknowledged.");
		char i[10];
		_itoa_s(airportList.size(), i, 10);
		Messager("Current in list: ");
		Messager(i);
		return true;
	}
	if (cmd.substr(0, 4) == ".SQS")
	{
		log("command \".SQS\" acknowledged.");
		char i[10];
		_itoa_s(sequence.size(), i, 10);
		Messager("Current in sequence:");
		Messager(i);
		return true;
	}
	if (cmd.substr(0, 4) == ".SQC")
	{
		log("command \".SQC\" acknowledged.");
		try
		{
			log("Spliting cmd");
			stringstream ss(cmd);
			char delim = ' ';
			string item;
			log("Clearing airport list");
			airportList.clear();
			log("Airport list cleared");
			while (getline(ss, item, delim))
			{
				if (!item.empty() && item[0] != '.')  // the command itself has been skiped here
				{
					log("Saving logon code: " + item + " to settings");
					SaveDataToSettings(PLUGIN_SETTING_KEY_LOGON_CODE, PLUGIN_SETTING_DESC_LOGON_CODE, item.c_str());
					log("Logon code saved.");
				}
			}
			Messager(MSG_LOGON_CODE_SAVED);
			if (!syncThreadFlag) dataSyncThread->detach();
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
	string cs = fp.GetCallsign();
	log("Initializing an new pointer for Sequence Node instance for " + cs);
	SeqN seqN = *new SeqN{ fp, AIRCRAFT_STATUS_NULL };
	log("Pushing an new instance to local sequence vector.");
	sequence.push_back(seqN);
}

void UniSequence::PatchStatus(CFlightPlan fp, int status)
{
	string cs = fp.GetCallsign();
	log("Attempting to patch status of " + cs);
#ifdef PATCH_WITH_LOGON_CODE
	if (!logonCode)
	{
		Messager(ERR_LOGON_CODE_NULLREF);
		return;
	}
#endif // PATCH_WITH_LOGON_CODE
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
					SyncSeq(seqObj[JSON_KEY_CALLSIGN], seqObj[JSON_KEY_STATUS]);
					SyncSeqNum(seqObj[JSON_KEY_CALLSIGN], seqNumber);
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
	log("Removing from local sequence");
	int offset = 0;
	for (auto& ac : sequence)
	{
		if (ac.fp.GetCallsign() == fp.GetCallsign())
		{
			char offsetStr[3];
			_itoa_s(offset, offsetStr, 10);
			log("Removing");
			sequence.erase(sequence.begin() + offset);
			log("removed");
			return;
		}
		offset++;
	}
	log("not in local sequence, nothing happend.");
}

void UniSequence::OnFunctionCall(int fId, const char* sItemString, POINT pt, RECT area)
{
	log("Function called by EuroScope");
	CFlightPlan fp;
	fp = FlightPlanSelectASEL();
	if (!fp.IsValid()) return;
	thread* reOrderThread;
	switch (fId)
	{
	case SEQUENCE_TAGITEM_FUNC_REORDER:
		log("Function: SEQUENCE_TAG_ITEM_FUNC_REORDER was called");
		OpenPopupEdit(area, SEQUENCE_TAGITEM_FUNC_REORDER_EDITED, "");
		break;
	case SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE:
		log("Function: SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE was called");
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
		log("Function: SEQUENCE_TAGITEM_FUNC_REORDER_EDITED was called");
#ifdef PATCH_WITH_LOGON_CODE
		if (!logonCode)
		{
			Messager(ERR_LOGON_CODE_NULLREF);
			return;
		}
#endif // PATCH_WITH_LOGON_CODE
		log("Creating an new thread for reorder request");
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
		log("Detaching reorder thread");
		reOrderThread->detach();
		log("reorder thread detached");
		break;
	case FUNC_SWITCH_TO_WFCR:
		log("Function: FUNC_SWITCH_TO_WFCR was called");
		PatchStatus(fp, AIRCRAFT_STATUS_WFCR);
		break;
	case FUNC_SWITCH_TO_CLRD:
		log("Function: FUNC_SWITCH_TO_CLRD was called");
		PatchStatus(fp, AIRCRAFT_STATUS_CLRD);
		break;
	case FUNC_SWITCH_TO_WFPU:
		log("Function: FUNC_SWITCH_TO_WFPU was called");
		PatchStatus(fp, AIRCRAFT_STATUS_WFPU);
		break;
	case FUNC_SWITCH_TO_PUSH:
		log("Function: FUNC_SWITCH_TO_PUSH was called");
		PatchStatus(fp, AIRCRAFT_STATUS_PUSH);
		break;
	case FUNC_SWITCH_TO_WFTX:
		log("Function: FUNC_SWITCH_TO_WFTX was called");
		PatchStatus(fp, AIRCRAFT_STATUS_WFTX);
		break;
	case FUNC_SWITCH_TO_TAXI:
		log("Function: FUNC_SWITCH_TO_TAXI was called");
		PatchStatus(fp, AIRCRAFT_STATUS_TAXI);
		break;
	case FUNC_SWITCH_TO_WFTO:
		log("Function: FUNC_SWITCH_TO_WFTO was called");
		PatchStatus(fp, AIRCRAFT_STATUS_WFTO);
		break;
	case FUNC_SWITCH_TO_TOGA:
		log("Function: FUNC_SWITCH_TO_TOGA was called");
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
	log("Airport not in list, adding");
	airportList.push_back(depAirport);
	log("Airport added.");
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
		log("Radar target's ground speed greater than 50kts, removing it.");
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