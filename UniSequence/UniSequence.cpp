#include "pch.h"
#include "UniSequence.h"
#include "websocket_endpoint.h"

using namespace std;
using nlohmann::json;

void UniSequence::log(string message)
{
	thread logThread([&] {
		lock_guard<mutex> guard(loglock);
		SYSTEMTIME sysTime = { 0 };
		GetSystemTime(&sysTime);
		if (!logStream.is_open()) logStream.open(LOG_FILE_NAME, ios::app);
		logStream << "[" << sysTime.wHour << ":" << sysTime.wMinute << ":" << sysTime.wSecond << "] " << message << endl;
		logStream.close();
		});
	logThread.detach();
}

void UniSequence::endLog()
{
	log("End log function was called, function will now stop and plugin unloaded.");
	if(logStream.is_open()) logStream.close();
}

void UniSequence::SyncSeq(string callsign, int status)
{
	lock_guard<mutex> guard(sequenceLock);
	log("Synchronizing sequence status for " + callsign);
	for (auto& seqN : sequence)
	{
		if (seqN.fp.GetCallsign() == callsign)
		{
			seqN.status = status;
			seqN.seqNumUpdated = true;
			log("Sync complete.");
			return;
		}
	}
	return;
}

// This function is not allowed to be called in non threads, otherwise it may cause access conflicts
void UniSequence::ClearUpdateFlag(string airport)
{
	lock_guard<mutex> guard(seqremovelock);
	// If the unit does not exist remotely, remove it from the local list
	for (auto& seqN : sequence)
	{
		if (!seqN.seqNumUpdated && seqN.fp.GetFlightPlanData().GetOrigin() == airport)
		{
			RemoveFromSeq(seqN.fp.GetCallsign());
			
		}
		else
		{
			seqN.seqNumUpdated = false;
		}
	}
}

void UniSequence::SyncSeqNum(string callsign, int seqNum)
{
	lock_guard<mutex> guard(sequenceLock);
	log("Synchronizing sequence number for " + callsign);
	for (auto& seqN : sequence)
	{
		if (seqN.fp.GetCallsign() == callsign)
		{
			seqN.sequenceNumber = seqNum;
			log("Sync complete.");
			seqN.seqNumUpdated = true;
			
			return;
		}
	}
	
	return;
}

void UniSequence::RemoveFromSeq(string callsign)
{
	lock_guard<mutex> guard(sequenceLock);
	for (auto& seqN : sequence)
	{
		if (seqN.fp.GetCallsign() == callsign)
		{
			seqN.status = 999;
			return;
		}
	}
}

UniSequence::UniSequence(void) : CPlugIn(
	EuroScopePlugIn::COMPATIBILITY_CODE,
	PLUGIN_NAME, PLUGIN_VER, PLUGIN_AUTHOR, PLUGIN_COPYRIGHT
)
{
	// Clear local log
	remove(LOG_FILE_NAME);
	log("UniSequence initializing");
	log("Attempting to register tag item type of \"Sequence\"");
	// Registering a Tag Object for EuroScope
	RegisterTagItemType("Sequence", SEQUENCE_TAGITEM_TYPE_CODE);
	RegisterTagItemFunction("Sequence Popup List", SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE);
	RegisterTagItemFunction("Sequence Reorder", SEQUENCE_TAGITEM_FUNC_REORDER);
#ifdef USE_WEBSOCKET
	wsSyncThread = new thread([&] {
		websocket_endpoint endpoint(this);
		string restfulVer = SERVER_RESTFUL_VER;
		while (syncThreadFlag)
		{
			for (auto& airport : airportList)
			{
				bool socketexist = false;
				for (auto& socket : socketList)
				{
					if (socket.icao == airport)
					{
						socketexist = true;
					}
				}
				if (!socketexist)
				{
					int id = endpoint.connect(WS_ADDRESS_PRC + restfulVer + airport + "/ws", airport);
					if (id >= 0)
					{
						socketList.push_back({ airport, id });
						Messager("Ws connection established, fetching data of " + airport);
					}
					else
					{
						Messager("Ws connection failed.");
					}
				}
			}
			// Delete airports that no longer exist in the local airport list
			for (auto& socket : socketList)
			{
				bool apexist = false;
				for (auto& airport : airportList)
				{
					if (airport == socket.icao) 
					{
						apexist = true;
					}
				}
				if (!apexist) endpoint.close(socket.socketId, websocketpp::close::status::normal, "Airport does not exist anymore");
			}
			// If there is a disconnection in ws, remove this socket from the list directly.
			int socketoffset = 0;
			for (auto& socket : socketList)
			{
				if (endpoint.get_metadata(socket.socketId).get()->get_status() == "")
				{
					socketList.erase(socketList.begin() + socketoffset);
				}
				socketoffset++;
			}
			this_thread::sleep_for(chrono::seconds(5));
		}
		});
	wsSyncThread->detach();
#else
	dataSyncThread = new thread([&] {
		httplib::Client requestClient(SERVER_ADDRESS_PRC);
		requestClient.set_connection_timeout(5, 0);
		while (syncThreadFlag)
		{
			try
			{
#ifdef PATCH_WITH_LOGON_CODE
				logonCode = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
				if (!logonCode) return;
#endif // PATCH_WITH_LOGON_CODE
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
						ClearUpdateFlag(airport);
					}
					else
					{
						Messager(httplib::to_string(result.error()));
					}
				}
				this_thread::sleep_for(chrono::seconds(timerInterval));
			}
			catch (const std::exception& e)
			{
				Messager(e.what());
			}
		}
	});
	Messager("Start synchronizing sequence data with server.");
	// detached anyway
	dataSyncThread->detach();
#endif // USE_WEBSOCKET
	// update check thread has been detached, so it's safe
	updateCheckThread = new thread([&] {
		httplib::Client updateReq(GITHUB_UPDATE);
		updateReq.set_connection_timeout(10, 0);
		Messager("Start updates check routine.");
		while (updateCheckFlag)
		{
			if (auto result = updateReq.Get(GITHUB_UPDATE_PATH))
			{
				json versionInfo = json::parse(result->body);
				string versionTag = versionInfo[0]["tag_name"];
				string versionName = versionInfo[0]["name"];
				string publishDate = versionInfo[0]["created_at"];
				if (versionTag != PLUGIN_VER)
				{
					Messager("Update is available! The latest version is: " + versionName + " " + versionTag + " | Publish date: " + publishDate);
				}
				// Now, an update prompt will only appear when and only when there is an update, without indicating that this plugin is the latest version
			}
			else
			{
				Messager("Error occured while checking updates - "+httplib::to_string(result.error()));
			}
			// Check for updates every 15 minutes.
			this_thread::sleep_for(chrono::minutes(15));
		}
		});
	updateCheckThread->detach();

	Messager("Initialization complete.");
}

UniSequence::~UniSequence(void)
{
	syncThreadFlag = false;
	updateCheckFlag = false;
	// Wait for the two threads above quit themselves nicely.
	this_thread::sleep_for(chrono::seconds(5));
}

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
					string lowercode = item.c_str();
					transform(lowercode.begin(), lowercode.end(), lowercode.begin(), ::tolower);
					SaveDataToSettings(PLUGIN_SETTING_KEY_LOGON_CODE, PLUGIN_SETTING_DESC_LOGON_CODE, lowercode.c_str());
					log("Logon code saved.");
				}
			}
			Messager(MSG_LOGON_CODE_SAVED);
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
	lock_guard<mutex> guard(sequenceLock);
	string cs = fp.GetCallsign();
	log("Initializing an new pointer for Sequence Node instance for " + cs);
	SeqN seqN = *new SeqN{ fp, AIRCRAFT_STATUS_NULL, false };
	log("Pushing an new instance to local sequence vector.");
	sequence.push_back(seqN);
	
	return;
}

void UniSequence::PatchStatus(CFlightPlan fp, int status)
{
	string cs = fp.GetCallsign();
	log("Attempting to patch status of " + cs);
	logonCode = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
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
			{JSON_KEY_STATUS, status}
		};
		httplib::Client patchReq(SERVER_ADDRESS_PRC);
		patchReq.set_connection_timeout(10, 0);
		string ap = fp.GetFlightPlanData().GetOrigin();
		if (auto result = patchReq.Patch(SERVER_RESTFUL_VER + ap + "/status", {{HEADER_LOGON_KEY, logonCode}}, reqBody.dump().c_str(), "application/json"))
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
				ClearUpdateFlag(ap);
			}
			else
			{
				if (result->status == 403)
				{
					Messager("Logon code verification failed.");
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
	lock_guard<mutex> guard(sequenceLock);
	log("Removing from local sequence");
	for (auto& ac : sequence)
	{
		if (ac.fp.GetCallsign() == fp.GetCallsign())
		{
			ac.status = 999;
			return;
		}
	}
	log("not in local sequence, nothing happend.");
	
	return;
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
		logonCode = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
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
			req.set_connection_timeout(10, 0);
			string ap = fp.GetFlightPlanData().GetOrigin();
			json reqBody = {
				{JSON_KEY_CALLSIGN, fp.GetCallsign()},
				{JSON_KEY_BEFORE, sItemString}
			};
			if (auto res = req.Patch(SERVER_RESTFUL_VER + ap + "/order", {{HEADER_LOGON_KEY, logonCode}}, reqBody.dump(), "application/json"))
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

					ClearUpdateFlag(ap);
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
	if (rt.GetGS() > 50) return;
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