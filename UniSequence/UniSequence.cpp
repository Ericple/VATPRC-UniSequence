#include "pch.h"
#include "UniSequence.h"
#include "websocket_endpoint.h"

using nlohmann::json;

void UniSequence::logToFile(std::string message)
{
	std::lock_guard<std::mutex> guard(loglock);
	if (!logStream.is_open()) {
		logStream.open(LOG_FILE_NAME, std::ios::app);
	}
	logStream << std::format("[{:%T}] {}", std::chrono::system_clock::now(), message) << std::endl;
	logStream.close();
}

void UniSequence::endLog()
{
	logToFile("End log function was called, function will now stop and plugin unloaded.");
	if(logStream.is_open()) logStream.close();
}

void UniSequence::UpdateBox()
{
	//MessageBox(NULL, "UniSequence 插件新版本已发布，请及时更新以获得稳定体验。", "更新可用", MB_OK);
}

#ifdef USE_WEBSOCKET
void UniSequence::initWsThread(void)
{
	wsSyncThread = new std::thread([&] {
		websocket_endpoint endpoint(this);
		std::string restfulVer = SERVER_RESTFUL_VER;
		while (syncThreadFlag)
		{
			for (auto& airport : airportList)
			{
				const auto& airport_socket = [&](const auto& socket) { return socket.icao == airport; };
				if (std::find_if(socketList.begin(), socketList.end(), airport_socket) == socketList.end())
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
				const auto& airport_socket = [&](const auto& airport) { return socket.icao == airport; };
				if (std::find_if(airportList.begin(), airportList.end(), airport_socket) == airportList.end()) {
					endpoint.close(socket.socketId, websocketpp::close::status::normal, "Airport does not exist anymore");
				}
			}
			// If there is a disconnection in ws, remove this socket from the list directly.
			socketList.erase(
				std::remove_if(
					socketList.begin(),
					socketList.end(),
					[&](const auto& socket) { return endpoint.get_metadata(socket.socketId).get()->get_status() == ""; }
				),
				socketList.end()
			);
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}
		});
	wsSyncThread->detach();
}
#else
void UniSequence::initDataSyncThread(void)
{
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
}
#endif
void UniSequence::initUpdateChckThread(void)
{
	updateCheckThread = new std::thread([&] {
		httplib::Client updateReq(GITHUB_UPDATE);
		updateReq.set_connection_timeout(10, 0);
		Messager("Start updates check routine.");
		if (updateCheckFlag) {
			if (auto result = updateReq.Get(GITHUB_UPDATE_PATH)) {
				json versionInfo = json::parse(result->body);
				if (versionInfo.contains("message")) {
					Messager("Error occured while checking updates.");
					Messager(versionInfo["message"]);
					return;
				}
				if (!versionInfo.is_array()) {
					Messager("Error occured while checking updates.");
					return;
				}
				std::string versionTag = versionInfo[0]["tag_name"];
				std::string versionName = versionInfo[0]["name"];
				std::string publishDate = versionInfo[0]["created_at"];
				if (versionTag != PLUGIN_VER) {
					if (showUpdateBox) {
						UpdateBox();
						showUpdateBox = false;
					}
					Messager("Update is available! The latest version is: " + versionName + " " + versionTag + " | Publish date: " + publishDate);
				}
				// Now, an update prompt will only appear when and only when there is an update, without indicating that this plugin is the latest version
			}
			else {
				Messager("Error occured while checking updates - " + httplib::to_string(result.error()));
			}
		}
		});
	updateCheckThread->detach();
}
UniSequence::UniSequence(void) : CPlugIn(
	EuroScopePlugIn::COMPATIBILITY_CODE,
	PLUGIN_NAME, PLUGIN_VER, PLUGIN_AUTHOR, PLUGIN_COPYRIGHT
)
{
	// Clear local log
	remove(LOG_FILE_NAME);
	logToFile("UniSequence initializing");
	logToFile("Attempting to register tag item type of \"Sequence\"");
	// Registering a Tag Object for EuroScope
	RegisterTagItemType(SEQUENCE_TAGITEM_TYPE_CODE_NAME, SEQUENCE_TAGITEM_TYPE_CODE);
	RegisterTagItemFunction(SEQUENCE_TAGFUNC_SWITCH_STATUS, SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE);
	RegisterTagItemFunction(SEQUENCE_TAGFUNC_REORDER_INPUT, SEQUENCE_TAGITEM_FUNC_REORDER);
	RegisterTagItemFunction(SEQUENCE_TAGFUNC_REORDER_SELECT, SEQUENCE_TAGITEM_FUNC_REORDER_SELECT);
	
	/*init communication thread*/
#ifdef USE_WEBSOCKET
	initWsThread();
#else
	initDataSyncThread();
#endif // USE_WEBSOCKET
	// update check thread has been detached, so it's safe
	initUpdateChckThread();

	Messager("Initialization complete.");
}

UniSequence::~UniSequence(void)
{
	syncThreadFlag = false;
	updateCheckFlag = false;
}

void UniSequence::Messager(std::string message)
{
	DisplayUserMessage("UniSequence", "system", message.c_str(),
		false, true, true, true, true);
	logToFile(message);
}
bool UniSequence::customCommandHanlder(std::string cmd)
{
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
		logToFile("command \".SQA\" acknowledged.");
		try
		{
			logToFile("Spliting cmd");
			std::stringstream ss(cmd);
			char delim = ' ';
			std::string item;
			logToFile("Clearing airport list");
			airportList.clear();
			logToFile("Airport list cleared");
			while (getline(ss, item, delim))
			{
				if (!item.empty() && item[0] != '.')  // the command itself has been skiped here
				{
					logToFile("Adding " + item + " to airport list");
					airportList.push_back(item);
				}
			}
			if (airportList.size() > 1) Messager("Airports saved.");
			if (airportList.size() == 1) Messager("Airport saved.");
			if (airportList.size() < 1) Messager("Airport cleared.");
		}
		catch (std::runtime_error const& e)
		{
			Messager("Error: " + *e.what());
		}
		return true;
	}
	if (cmd.substr(0, 4) == ".SQP")
	{
		logToFile("command \".SQP\" acknowledged.");
		Messager("Current in list: ");
		Messager(std::to_string(airportList.size()));
		return true;
	}
	if (cmd.substr(0, 4) == ".SQC")
	{
		logToFile("command \".SQC\" acknowledged.");
		try
		{
			logToFile("Spliting cmd");
			std::stringstream ss(cmd);
			char delim = ' ';
			std::string item;
			logToFile("Clearing airport list");
			airportList.clear();
			logToFile("Airport list cleared");
			while (getline(ss, item, delim))
			{
				if (!item.empty() && item[0] != '.')  // the command itself has been skiped here
				{
					logToFile("Saving logon code: " + item + " to settings");
					std::string lowercode = item.c_str();
					transform(lowercode.begin(), lowercode.end(), lowercode.begin(), ::tolower);
					SaveDataToSettings(PLUGIN_SETTING_KEY_LOGON_CODE, PLUGIN_SETTING_DESC_LOGON_CODE, lowercode.c_str());
					logToFile("Logon code saved.");
				}
			}
			Messager(MSG_LOGON_CODE_SAVED);
		}
		catch (std::runtime_error const& e)
		{
			Messager("Error: " + *e.what());
		}
		return true;
	}

	return false;
}
bool UniSequence::OnCompileCommand(const char* sCommandLine)
{
	std::string cmd = sCommandLine;
	std::regex unisRegex(".");
	logToFile("Command received: " + cmd);
	// transform cmd to uppercase in order for identify
	transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
	return customCommandHanlder(cmd);
}

void UniSequence::PatchStatus(CFlightPlan fp, int status)
{
	std::string cs = fp.GetCallsign();
	logToFile("Attempting to patch status of " + cs);
	logonCode = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
#ifdef PATCH_WITH_LOGON_CODE
	if (!logonCode)
	{
		Messager(ERR_LOGON_CODE_NULLREF);
		return;
	}
#endif // PATCH_WITH_LOGON_CODE
	std::thread patchThread([fp, status, this] {
		json reqBody = {
			{JSON_KEY_CALLSIGN, fp.GetCallsign()},
			{JSON_KEY_STATUS, status}
		};
		httplib::Client patchReq(SERVER_ADDRESS_PRC);
		patchReq.set_connection_timeout(10, 0);
		std::string ap = fp.GetFlightPlanData().GetOrigin();
		if (auto result = patchReq.Patch(SERVER_RESTFUL_VER + ap + "/status", {{HEADER_LOGON_KEY, logonCode}}, reqBody.dump().c_str(), "application/json"))
		{
			if (result->status == 200)
			{
				setQueueJson(ap, result->body);
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

void UniSequence::setQueueJson(std::string airport, std::string content)
{
	std::lock_guard<std::mutex> guard(j_queueLock);
	j_queueCaches[airport] = json::parse(content)["data"];
}

SeqN* UniSequence::GetFromList(CFlightPlan fp)
{
	int seqNum = 1;
	int status = AIRCRAFT_STATUS_NULL;
	std::string airport = fp.GetFlightPlanData().GetOrigin();
	json list = j_queueCaches[airport];
	for (auto& node : list)
	{
		if (node["callsign"] == fp.GetCallsign())
		{
			status = node["status"];
			return new SeqN{node["callsign"], fp.GetFlightPlanData().GetOrigin(), status, seqNum, false};
		}
		seqNum++;
	}
	return nullptr;
}

void UniSequence::OnFunctionCall(int fId, const char* sItemString, POINT pt, RECT area)
{
	logToFile(std::format("Function (ID: {}) is called by EuroScope, param: {}", fId, sItemString));
	CFlightPlan fp;
	fp = FlightPlanSelectASEL();
	if (!fp.IsValid()) return;
	SeqN* thisAc = GetFromList(fp);
	std::string ap = fp.GetFlightPlanData().GetOrigin();
	std::string beforeKey;
	std::thread* reOrderThread;
	json list = j_queueCaches[ap];
	switch (fId)
	{
	case SEQUENCE_TAGITEM_FUNC_REORDER:
		logToFile("Function: SEQUENCE_TAG_ITEM_FUNC_REORDER was called");
		OpenPopupEdit(area, SEQUENCE_TAGITEM_FUNC_REORDER_EDITED, "");
		break;
	case SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE:
		logToFile("Function: SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE was called");
		OpenPopupList(area, fp.GetCallsign(), 2);
		AddPopupListElement(STATUS_DESC_WFCR, "", FUNC_SWITCH_TO_WFCR);
		AddPopupListElement(STATUS_DESC_CLRD, "", FUNC_SWITCH_TO_CLRD);
		AddPopupListElement(STATUS_DESC_WFPU, "", FUNC_SWITCH_TO_WFPU);
		AddPopupListElement(STATUS_DESC_PUSH, "", FUNC_SWITCH_TO_PUSH);
		AddPopupListElement(STATUS_DESC_WFTX, "", FUNC_SWITCH_TO_WFTX);
		AddPopupListElement(STATUS_DESC_TAXI, "", FUNC_SWITCH_TO_TAXI);
		AddPopupListElement(STATUS_DESC_WFTO, "", FUNC_SWITCH_TO_WFTO);
		AddPopupListElement(STATUS_DESC_TKOF, "", FUNC_SWITCH_TO_TOGA);
		break;
	case SEQUENCE_TAGITEM_FUNC_REORDER_SELECT:
		if (!thisAc) return;
		if (thisAc->status == AIRCRAFT_STATUS_NULL) return;
		OpenPopupList(area, "PUT BEFORE", 2);
		AddPopupListElement(SEQUENCE_TAGFUNC_REORDER_TOPKEY, "", SEQUENCE_TAGITEM_FUNC_REORDER_EDITED);
		for (auto& aircraft : list)
		{
			std::string cs = aircraft["callsign"];
			if (aircraft["status"] == thisAc->status && cs != thisAc->callsign) AddPopupListElement(cs.c_str(), "", SEQUENCE_TAGITEM_FUNC_REORDER_EDITED);
		}
		break;
	case SEQUENCE_TAGITEM_FUNC_REORDER_EDITED:
		if (!thisAc) return;
		logToFile("Function: SEQUENCE_TAGITEM_FUNC_REORDER_EDITED was called");
		logonCode = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
#ifdef PATCH_WITH_LOGON_CODE
		if (!logonCode)
		{
			Messager(ERR_LOGON_CODE_NULLREF);
			return;
		}
#endif // PATCH_WITH_LOGON_CODE
		if (sItemString == SEQUENCE_TAGFUNC_REORDER_TOPKEY)
		{
			beforeKey = "-1";
		}
		else
		{
			beforeKey = sItemString;
		}
		logToFile("Creating an new thread for reorder request");
		reOrderThread = new std::thread([beforeKey, fp, this] {
			httplib::Client req(SERVER_ADDRESS_PRC);
			req.set_connection_timeout(10, 0);
			std::string ap = fp.GetFlightPlanData().GetOrigin();
			json reqBody = {
				{JSON_KEY_CALLSIGN, fp.GetCallsign()},
				{JSON_KEY_BEFORE, beforeKey}
			};
			if (auto res = req.Patch(SERVER_RESTFUL_VER + ap + "/order", {{HEADER_LOGON_KEY, logonCode}}, reqBody.dump(), "application/json"))
			{
				if (res->status == 200)
				{
					setQueueJson(ap, res->body);
				}
			}
			else
			{
				Messager(httplib::to_string(res.error()));
			}
			});
		logToFile("Detaching reorder thread");
		reOrderThread->detach();
		logToFile("reorder thread detached");
		break;
	case FUNC_SWITCH_TO_WFCR:
		logToFile("Function: FUNC_SWITCH_TO_WFCR was called");
		PatchStatus(fp, AIRCRAFT_STATUS_WFCR);
		break;
	case FUNC_SWITCH_TO_CLRD:
		logToFile("Function: FUNC_SWITCH_TO_CLRD was called");
		PatchStatus(fp, AIRCRAFT_STATUS_CLRD);
		break;
	case FUNC_SWITCH_TO_WFPU:
		logToFile("Function: FUNC_SWITCH_TO_WFPU was called");
		PatchStatus(fp, AIRCRAFT_STATUS_WFPU);
		break;
	case FUNC_SWITCH_TO_PUSH:
		logToFile("Function: FUNC_SWITCH_TO_PUSH was called");
		PatchStatus(fp, AIRCRAFT_STATUS_PUSH);
		break;
	case FUNC_SWITCH_TO_WFTX:
		logToFile("Function: FUNC_SWITCH_TO_WFTX was called");
		PatchStatus(fp, AIRCRAFT_STATUS_WFTX);
		break;
	case FUNC_SWITCH_TO_TAXI:
		logToFile("Function: FUNC_SWITCH_TO_TAXI was called");
		PatchStatus(fp, AIRCRAFT_STATUS_TAXI);
		break;
	case FUNC_SWITCH_TO_WFTO:
		logToFile("Function: FUNC_SWITCH_TO_WFTO was called");
		PatchStatus(fp, AIRCRAFT_STATUS_WFTO);
		break;
	case FUNC_SWITCH_TO_TOGA:
		logToFile("Function: FUNC_SWITCH_TO_TOGA was called");
		PatchStatus(fp, AIRCRAFT_STATUS_TOGA);
		break;
	default:
		break;
	}
}

auto UniSequence::AddAirportIfNotExist(const std::string& dep_airport) -> void
{
	const auto& is_same_airport = [&](auto& airport) { 
		return airport == dep_airport;
	};
	if (std::find_if(airportList.cbegin(), airportList.cend(), is_same_airport) == airportList.cend()) {
		logToFile(std::format("Airport {} is not in the list.", dep_airport));
		airportList.push_back(dep_airport);
		logToFile(std::format("Airport {} added.", dep_airport));
	}
}

void UniSequence::OnGetTagItem(CFlightPlan fp, CRadarTarget rt, int itemCode, int tagData,
	char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize)
{
	// check if item code is what we need to handle
	if (itemCode != SEQUENCE_TAGITEM_TYPE_CODE) return;
	// check if this tag is valid
	if (!fp.IsValid()) return;
	// remove aircraft if it's taking off
	if (rt.IsValid() && rt.GetGS() > 50) return;
	// check if the departure airport of this fp is in the airport list
	std::string depAirport = fp.GetFlightPlanData().GetOrigin();
	AddAirportIfNotExist(depAirport);
	int seqNum = 1;
	int status = AIRCRAFT_STATUS_NULL;
	json list = j_queueCaches[depAirport];
	bool seqadd = true;
	for (auto& node : list)
	{
		if (node["callsign"] == fp.GetCallsign())
		{
			status = node["status"];
			seqadd = false;
		}
		if (seqadd) seqNum++;
	}
	// you won't want to say "Your sequence number is one hundred and fivty nine" :D
	if (seqNum > 99) seqNum = 99;
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