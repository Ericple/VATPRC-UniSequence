#include "pch.h"
#include "UniSequence.h"
#include "websocket_endpoint.h"

using nlohmann::json;

auto UniSequence::LogMessage(const std::string& message, const int& level) -> void
{
	if (level >= LOG_LEVEL_NOTE) {
		DisplayUserMessage("UniSequence", nullptr, message.c_str(),
			true, true, true, true, false);
	}
	else if (level >= LOG_LEVEL_INFO) {
		DisplayUserMessage("Message", "UniSequence", message.c_str(),
			true, false, false, false, false);
	}
	// LOG_LEVEL_DEBG
	std::lock_guard<std::mutex> guard(log_lock_);
	if (log_file.size()) {
		log_stream_.open(log_file, std::ios::app);
		if (log_stream_.is_open()) {
			log_stream_ << std::format("[{:%T}] {}", std::chrono::system_clock::now(), message) << std::endl;
			log_stream_.close();
		}
	}
}

auto UniSequence::InitWsThread(void) -> void
{
	sync_thread = std::jthread([&](std::stop_token stoken) {
		websocket_endpoint endpoint(this);
		std::mutex this_thread_lock_;
		while (true)
		{
			std::unique_lock threadLock(this_thread_lock_);
			std::condition_variable_any().wait_for(threadLock, stoken, std::chrono::seconds(5),
				[] { return false; });
			if (stoken.stop_requested()) return;
			std::shared_lock<std::shared_mutex> airportLock(airport_list_lock_);
			std::unique_lock<std::shared_mutex> socketLock(socket_list_lock_);
			for (auto& airport : airport_list_)
			{
				if (!socket_list_.contains(airport))
				{
					int id = endpoint.connect(std::format("{}{}{}/ws", WS_ADDRESS_PRC, SERVER_RESTFUL_VER, airport), airport);
					if (id >= 0)
					{
						socket_list_.insert({ airport, id });
						LogMessage(std::format("Ws connection established, fetching data of {}", airport));
					}
					else
					{
						LogMessage("Ws connection failed.");
					}
				}
			}
			// Delete airports that no longer exist in the local airport list
			for (auto socket_itr = socket_list_.begin(); socket_itr != socket_list_.end();) {
				if (!airport_list_.contains(socket_itr->first)) {
					// If there is a disconnection in ws, remove this socket from the list directly.
					endpoint.close(socket_itr->second, websocketpp::close::status::normal, "Airport does not exist anymore");
					socket_itr = socket_list_.erase(socket_itr);
				}
				else {
					socket_itr++;
				}
			}
			// unlocked
		}
		});
}

auto UniSequence::InitUpdateChckThread(void) -> void
{
	update_check_thread = std::jthread([&](std::stop_token stoken) {
		httplib::Client updateReq(GITHUB_UPDATE);
		updateReq.set_connection_timeout(10, 0);
		std::mutex this_thread_lock_;
		int retryTime = 0;
		while (retryTime < 3) { // allows maximum 3 retries
			retryTime++;
			LogMessage(std::format("Checking for update, attmept #{}.", retryTime));
			std::unique_lock threadLock(this_thread_lock_);
			bool stopped = std::condition_variable_any().wait_for(threadLock, stoken, std::chrono::seconds(10),
				[stoken] { return stoken.stop_requested(); });
			if (stopped) return;
			if (auto result = updateReq.Get(GITHUB_UPDATE_PATH)) {
				json versionInfo = json::parse(result->body);
				if (versionInfo.contains("message")) {
					LogMessage("Error occured while checking updates.");
					LogMessage(versionInfo["message"]);
					continue;
				}
				if (!versionInfo.is_array()) {
					LogMessage("Error occured while checking updates.");
					continue;
				}
				std::string versionTag = versionInfo[0]["tag_name"];
				std::string versionName = versionInfo[0]["name"];
				std::string publishDate = versionInfo[0]["created_at"];
				if (versionTag != PLUGIN_VER) {
					LogMessage(std::format("Update is available! The latest version is: {} {} | Publish date: {}", versionName, versionTag, publishDate), LOG_LEVEL_INFO);
				}
				else {
					LogMessage("No updates found.");
				}
				return; // update check completes
			}
			else {
				LogMessage(std::format("Error occured while checking updates - {}", httplib::to_string(result.error())));
			}
		}
		LogMessage("Error occured while checking updates.", LOG_LEVEL_INFO);
		});
}

auto UniSequence::InitPatchThread(void) -> void
{
	patch_request_thread = std::jthread([&](std::stop_token stoken) {
		std::mutex this_thread_lock_;
		while (!stoken.stop_requested())
		{
			std::unique_lock threadLock(this_thread_lock_);
			bool is_queueing = patch_status_condvar.wait(threadLock, stoken,
				[this, stoken] {
					std::lock_guard queueLock(patch_request_queue_lock_);
					return !patch_request_queue.empty() && !stoken.stop_requested();
				});
			if (!is_queueing) continue;
			// deals with queue and send request sequentially
			LogMessage("Getting the front of patch queue.");
			std::unique_lock patchLock(patch_request_queue_lock_);
			PRequest pReq = patch_request_queue.front();
			patch_request_queue.pop();
			patchLock.unlock();
			// making request
			LogMessage(std::format("Making patch request: airport={}, type={}, body={}",
				pReq.airport, pReq.reqType, pReq.reqBody.dump()));
			httplib::Client patchReq(SERVER_ADDRESS_PRC);
			patchReq.set_connection_timeout(10, 0);
			std::string ap = pReq.airport;
			if (auto result = patchReq.Patch(std::format("{}{}{}", SERVER_RESTFUL_VER, ap, pReq.reqType), { {HEADER_LOGON_KEY, logon_code_} }, pReq.reqBody.dump(), "application/json"))
			{
				if (result->status == 200)
				{
					LogMessage("Patch successful with return. Setting queue from response.");
					SetQueueFromJson(ap, result->body);
				}
				else if (result->status == 403)
				{
					LogMessage("Logon code verification failed.", LOG_LEVEL_NOTE);
				}
			}
			else
			{
				LogMessage(httplib::to_string(result.error()));
			}
		}
		});
}

auto UniSequence::InitializeLogEnv(void) -> void
{
	// find dll path
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	HMODULE pluginModule = AfxGetInstanceHandle();
	TCHAR pBuffer[MAX_PATH] = { 0 };
	GetModuleFileName(pluginModule, pBuffer, sizeof(pBuffer) / sizeof(TCHAR) - 1);
	std::string currentPath = pBuffer;
	log_file = currentPath.substr(0, currentPath.find_last_of("\\/") + 1) + LOG_FILE_NAME;
	remove(log_file.c_str());
	LogMessage("UniSequence initializing");
}
auto UniSequence::InitTagItem(void) -> void
{
	RegisterTagItemType(SEQUENCE_TAGITEM_TYPE_CODE_NAME, SEQUENCE_TAGITEM_TYPE_CODE);
	RegisterTagItemFunction(SEQUENCE_TAGFUNC_SWITCH_STATUS, SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE);
	RegisterTagItemFunction(SEQUENCE_TAGFUNC_REORDER_INPUT, SEQUENCE_TAGITEM_FUNC_REORDER);
	RegisterTagItemFunction(SEQUENCE_TAGFUNC_REORDER_SELECT, SEQUENCE_TAGITEM_FUNC_REORDER_SELECT);
	LogMessage("Tag item type and functions are registered");
}
UniSequence::UniSequence(void) : CPlugIn(
	EuroScopePlugIn::COMPATIBILITY_CODE,
	PLUGIN_NAME, PLUGIN_VER, PLUGIN_AUTHOR, PLUGIN_COPYRIGHT
)
{
	InitializeLogEnv();
	// Registering a Tag Object for EuroScope
	InitTagItem();

	// Start threads
#ifdef USE_WEBSOCKET
	InitWsThread();
#endif // USE_WEBSOCKET
	InitUpdateChckThread();
	InitPatchThread();

	LogMessage("Initialization complete.", LOG_LEVEL_INFO);
}

UniSequence::~UniSequence(void)
{
	sync_thread.request_stop();
	update_check_thread.request_stop();
	patch_request_thread.request_stop();
	sync_thread.join();
	update_check_thread.join();
	patch_request_thread.join();
}

auto UniSequence::CommandMatch(const std::string& cmd, const char* target) -> bool
{
	return cmd.substr(0, strlen(target)) == target;
}
auto UniSequence::CustomCommandHanlder(std::string cmd) -> bool
{
	// display copyright and help link information
	if (CommandMatch(cmd, ".UNIS"))
	{
		LogMessage("UniSequence Plugin By VATPRC DIVISION.", LOG_LEVEL_NOTE);
		LogMessage("Author: Ericple Garrison", LOG_LEVEL_NOTE);
		LogMessage("For help and bug report, refer to https://github.com/Ericple/VATPRC-UniSequence", LOG_LEVEL_NOTE);
		return true;
	}
	// Set airports to be listened
	if (CommandMatch(cmd, ".SQA"))
	{
		LogMessage("command \".SQA\" acknowledged.");
		std::unique_lock<std::shared_mutex> airportLock(airport_list_lock_);
		std::stringstream ss(cmd);
		char delim = ' ';
		std::string item;
		airport_list_.clear();
		LogMessage("Airport list cleared");
		std::string msg = "Active airports: ";
		ss >> item; // skip .sqa here
		while (ss >> item)
		{
			LogMessage(std::format("Adding {} to airport list", item));
			airport_list_.insert(item);
			msg += item + " ";
		}
		LogMessage(msg, LOG_LEVEL_NOTE);
		return true;
	}
	if (CommandMatch(cmd, ".SQP"))
	{
		LogMessage("command \".SQP\" acknowledged.");
		std::shared_lock<std::shared_mutex> airportLock(airport_list_lock_);
		std::string msg = "Active airports: ";
		for (const auto& a : airport_list_) {
			msg += a + " ";
		}
		LogMessage(msg, LOG_LEVEL_NOTE);
		return true;
	}
	if (CommandMatch(cmd, ".SQC"))
	{
		LogMessage("command \".SQC\" acknowledged.");
		std::stringstream ss(cmd);
		char delim = ' ';
		std::string item;
		getline(ss, item, delim); // skip .sqc here
		if (getline(ss, item, delim) && item.size()) {
			std::string lowercode;
			transform(item.begin(), item.end(), std::back_inserter(lowercode), ::tolower);
			SaveDataToSettings(PLUGIN_SETTING_KEY_LOGON_CODE, PLUGIN_SETTING_DESC_LOGON_CODE, lowercode.c_str());
			LogMessage(std::format("Logon code {} is saved to settings", lowercode));
			LogMessage(MSG_LOGON_CODE_SAVED, LOG_LEVEL_INFO);
			return true;
		}
	}
	return false;
}
auto UniSequence::OnCompileCommand(const char* sCommandLine) -> bool
{
	std::string cmd = sCommandLine;
	std::regex unisRegex(".");
	LogMessage(std::format("Command received: ", cmd));
	transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
	return CustomCommandHanlder(cmd);
}

auto UniSequence::PatchAircraftStatus(CFlightPlan fp, int status) -> void
{
	std::string cs = fp.GetCallsign();
	LogMessage(std::format("Attempting to patch status of {}", cs));
	auto logon_code_setting = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
	logon_code_ = logon_code_setting != nullptr ? logon_code_setting : "";
	if (!logon_code_.size())
	{
		LogMessage(ERR_LOGON_CODE_NULLREF, LOG_LEVEL_NOTE);
		return;
	}
	std::unique_lock patchLock(patch_request_queue_lock_);
	PRequest pReq(fp, status);
	patch_request_queue.push(pReq);
	patchLock.unlock();
	patch_status_condvar.notify_all();
}

auto UniSequence::SetQueueFromJson(const std::string& airport, const std::string& content) -> void
{
	std::unique_lock<std::shared_mutex> lock(queue_cache_lock_);
	queue_caches_[airport] = json::parse(content)["data"];
}

auto UniSequence::GetManagedAircraft(CFlightPlan fp) -> std::shared_ptr<SeqNode>
{
	int seqNum = 1;
	int status = AIRCRAFT_STATUS_NULL;
	std::string airport = fp.GetFlightPlanData().GetOrigin();
	json list;
	{
		std::shared_lock<std::shared_mutex> lock(queue_cache_lock_);
		list = queue_caches_[airport];
	}
	std::shared_ptr<SeqNode> resNode;
	for (auto& node : list)
	{
		if (node["callsign"] == fp.GetCallsign())
		{
			status = node["status"];
			resNode = std::make_shared<SeqNode>(node["callsign"], fp.GetFlightPlanData().GetOrigin(), status, seqNum, false);
			break;
		}
		seqNum++;
	}
	return resNode;
}

auto UniSequence::ReorderAircraftByEdit(RECT area) -> void
{
	LogMessage("Function: SEQUENCE_TAG_ITEM_FUNC_REORDER was called");
	OpenPopupEdit(area, SEQUENCE_TAGITEM_FUNC_REORDER_EDITED, "");
}

auto UniSequence::OpenStatusAsignMenu(RECT area, CFlightPlan fp) -> void
{
	LogMessage("Function: SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE was called");
	OpenPopupList(area, fp.GetCallsign(), 2);
	AddPopupListElement(STATUS_DESC_WFCR, "", FUNC_SWITCH_TO_WFCR);
	AddPopupListElement(STATUS_DESC_CLRD, "", FUNC_SWITCH_TO_CLRD);
	AddPopupListElement(STATUS_DESC_WFPU, "", FUNC_SWITCH_TO_WFPU);
	AddPopupListElement(STATUS_DESC_PUSH, "", FUNC_SWITCH_TO_PUSH);
	AddPopupListElement(STATUS_DESC_WFTX, "", FUNC_SWITCH_TO_WFTX);
	AddPopupListElement(STATUS_DESC_TAXI, "", FUNC_SWITCH_TO_TAXI);
	AddPopupListElement(STATUS_DESC_WFTO, "", FUNC_SWITCH_TO_WFTO);
	AddPopupListElement(STATUS_DESC_TKOF, "", FUNC_SWITCH_TO_TOGA);
}

auto UniSequence::ReorderAircraftBySelect(std::shared_ptr<SeqNode> thisAc, RECT area, const std::string& ap) -> void
{
	json list;
	{
		std::shared_lock<std::shared_mutex> lock(queue_cache_lock_);
		list = queue_caches_[ap];
	}
	if (thisAc == nullptr) return;
	if (thisAc->status == AIRCRAFT_STATUS_NULL) return;
	OpenPopupList(area, "PUT BEFORE", 2);
	AddPopupListElement(SEQUENCE_TAGFUNC_REORDER_TOPKEY, "", SEQUENCE_TAGITEM_FUNC_REORDER_EDITED);
	for (auto& aircraft : list)
	{
		std::string cs = aircraft["callsign"];
		if (aircraft["status"] == thisAc->status && cs != thisAc->callsign) AddPopupListElement(cs.c_str(), "", SEQUENCE_TAGITEM_FUNC_REORDER_EDITED);
	}
}

auto UniSequence::ReorderAircraftEditHandler(std::shared_ptr<SeqNode> thisAc, CFlightPlan fp, const char* sItemString) -> void
{
	std::string beforeKey = sItemString;
	if (thisAc == nullptr) return;
	LogMessage("Function: SEQUENCE_TAGITEM_FUNC_REORDER_EDITED was called");
	auto logon_code_setting = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
	logon_code_ = logon_code_setting != nullptr ? logon_code_setting : "";
	if (!logon_code_.size())
	{
		LogMessage(ERR_LOGON_CODE_NULLREF, LOG_LEVEL_NOTE);
		return;
	}
	if (beforeKey == SEQUENCE_TAGFUNC_REORDER_TOPKEY)
	{ // to the moon
		beforeKey = "-1";
	}
	std::unique_lock patchLock(patch_request_queue_lock_);
	PRequest pReq(fp, beforeKey);
	patch_request_queue.push(pReq);
	patchLock.unlock();
	patch_status_condvar.notify_all();
}

auto UniSequence::OnFunctionCall(int fId, const char* sItemString, POINT pt, RECT area) -> void
{
	LogMessage(std::format("Function (ID: {}) is called by EuroScope, param: {}", fId, sItemString));
	CFlightPlan fp;
	fp = FlightPlanSelectASEL();
	if (!fp.IsValid()) return;
	auto thisAc = GetManagedAircraft(fp);
	auto ap = fp.GetFlightPlanData().GetOrigin();

	switch (fId)
	{
	case SEQUENCE_TAGITEM_FUNC_REORDER:
		ReorderAircraftByEdit(area);
		break;
	case SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE:
		OpenStatusAsignMenu(area, fp);
		break;
	case SEQUENCE_TAGITEM_FUNC_REORDER_SELECT:
		ReorderAircraftBySelect(thisAc, area, ap);
		break;
	case SEQUENCE_TAGITEM_FUNC_REORDER_EDITED:
		ReorderAircraftEditHandler(thisAc, fp, sItemString);
		break;
	case FUNC_SWITCH_TO_WFCR:
		LogMessage("Function: FUNC_SWITCH_TO_WFCR was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_WFCR);
		break;
	case FUNC_SWITCH_TO_CLRD:
		LogMessage("Function: FUNC_SWITCH_TO_CLRD was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_CLRD);
		break;
	case FUNC_SWITCH_TO_WFPU:
		LogMessage("Function: FUNC_SWITCH_TO_WFPU was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_WFPU);
		break;
	case FUNC_SWITCH_TO_PUSH:
		LogMessage("Function: FUNC_SWITCH_TO_PUSH was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_PUSH);
		break;
	case FUNC_SWITCH_TO_WFTX:
		LogMessage("Function: FUNC_SWITCH_TO_WFTX was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_WFTX);
		break;
	case FUNC_SWITCH_TO_TAXI:
		LogMessage("Function: FUNC_SWITCH_TO_TAXI was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_TAXI);
		break;
	case FUNC_SWITCH_TO_WFTO:
		LogMessage("Function: FUNC_SWITCH_TO_WFTO was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_WFTO);
		break;
	case FUNC_SWITCH_TO_TOGA:
		LogMessage("Function: FUNC_SWITCH_TO_TOGA was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_TOGA);
		break;
	default:
		break;
	}
}

auto UniSequence::OnFlightPlanControllerAssignedDataUpdate(CFlightPlan fp, int DataType) -> void
{
	if (!fp.IsValid() || !fp.GetTrackingControllerIsMe()) return; // only tracked ac will be updated
	if (DataType == CTR_DATA_TYPE_GROUND_STATE || DataType == CTR_DATA_TYPE_CLEARENCE_FLAG) {
		std::string state = fp.GetGroundState();
		bool cleared = fp.GetClearenceFlag();
		LogMessage(std::format("Handling CTR_DATA update event, TYPE: {} state: {}, flag: {}", DataType, state, cleared));
		if (state.empty()) {
			if (!cleared) {
				PatchAircraftStatus(fp, AIRCRAFT_STATUS_WFCR);
			}
			else {
				PatchAircraftStatus(fp, AIRCRAFT_STATUS_CLRD);
			}
		}
		else if (state == "STUP") {
			PatchAircraftStatus(fp, AIRCRAFT_STATUS_PUSH);
		}
		else if (state == "PUSH") {
			PatchAircraftStatus(fp, AIRCRAFT_STATUS_PUSH);
		}
		else if (state == "TAXI") {
			PatchAircraftStatus(fp, AIRCRAFT_STATUS_TAXI);
		}
		else if (state == "DEPA") {
			PatchAircraftStatus(fp, AIRCRAFT_STATUS_TOGA);
		}
	}
}

auto UniSequence::AddAirportIfNotExist(const std::string& dep_airport) -> void
{
	const auto& is_same_airport = [&](auto& airport) {
		return airport == dep_airport;
		};
	std::unique_lock<std::shared_mutex> airportLock(airport_list_lock_);
	if (std::find_if(airport_list_.cbegin(), airport_list_.cend(), is_same_airport) == airport_list_.cend()) {
		LogMessage(std::format("Airport {} is not in the list.", dep_airport));
		airport_list_.insert(dep_airport);
		LogMessage(std::format("Airport {} added.", dep_airport));
	}
}

auto UniSequence::OnGetTagItem(CFlightPlan fp, CRadarTarget rt, int itemCode, int tagData,
	char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize) -> void
{
	if (itemCode != SEQUENCE_TAGITEM_TYPE_CODE || !fp.IsValid()) return; // Only deals with valid fp requesting corresponding item

	std::string depAirport = fp.GetFlightPlanData().GetOrigin();// Get departure airport from flight plan object

	AddAirportIfNotExist(depAirport);// Check if the departure airport of this flight plan is in the managed airport list

	int seqNum = 1;// For iterate use below

	int status = AIRCRAFT_STATUS_NULL;// For iterate use below

	bool seqadd = true;// For iterate use below

	json list; // For iterate use below
	{
		std::shared_lock<std::shared_mutex> lock(queue_cache_lock_);
		list = queue_caches_[depAirport];
	}

	for (auto& node : list)// Iterate each node in queue fetched from server
	{
		if (node["callsign"] == fp.GetCallsign())// Synchronize status from server
		{
			status = node["status"];
			seqadd = false;
		}
		if (seqadd) seqNum++;
	}

	if (seqNum > 99) seqNum = 99;// you won't want to say "Your sequence number is one hundred and fivty nine"

	int bufferSize = strlen(STATUS_TEXT_PLACE_HOLDER) + 1;// Get buffer length from status

	switch (status)// Print correspond status string to sItemString
	{
	case AIRCRAFT_STATUS_NULL:
		sprintf_s(sItemString, 1, "%s", STATUS_TEXT_NULL);
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
