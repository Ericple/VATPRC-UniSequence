#include "pch.h"
#include "UniSequence.h"
#include "websocket_endpoint.h"

using nlohmann::json;

auto UniSequence::LogToFile(std::string message) -> void
{
	std::lock_guard<std::mutex> guard(log_lock_);
	if (!log_stream_.is_open()) {
		log_stream_.open(LOG_FILE_NAME, std::ios::app);
	}
	log_stream_ << std::format("[{:%T}] {}", std::chrono::system_clock::now(), message) << std::endl;
	log_stream_.close();
}

auto UniSequence::InitWsThread(void) -> void
{
	std::thread([&] {
		websocket_endpoint endpoint(this);
		std::string restfulVer = SERVER_RESTFUL_VER;
		while (sync_thread_flag_)
		{
			{
				std::shared_lock<std::shared_mutex> airportLock(airport_list_lock_);
				std::unique_lock<std::shared_mutex> socketLock(socket_list_lock_);
				for (auto& airport : airport_list_)
				{
					const auto& airport_socket = [&](const auto& socket) { return socket.icao == airport; };
					if (std::find_if(socket_list_.begin(), socket_list_.end(), airport_socket) == socket_list_.end())
					{

						int id = endpoint.connect(std::format("{}{}{}/ws", WS_ADDRESS_PRC, restfulVer, airport), airport);
						if (id >= 0)
						{
							socket_list_.push_back({ airport, id });
							LogToES(std::format("Ws connection established, fetching data of {}", airport));
						}
						else
						{
							LogToES("Ws connection failed.");
						}
					}
				}
				// Delete airports that no longer exist in the local airport list
				for (auto& socket : socket_list_)
				{
					const auto& airport_socket = [&](const auto& airport) { return socket.icao == airport; };
					if (std::find_if(airport_list_.begin(), airport_list_.end(), airport_socket) == airport_list_.end()) {
						endpoint.close(socket.socketId, websocketpp::close::status::normal, "Airport does not exist anymore");
					}
				}
				// If there is a disconnection in ws, remove this socket from the list directly.
				socket_list_.erase(
					std::remove_if(
						socket_list_.begin(),
						socket_list_.end(),
						[&](const auto& socket) { return endpoint.get_metadata(socket.socketId).get()->get_status() == ""; }
					),
					socket_list_.end()
				);
			} // unlocked
			std::this_thread::sleep_for(std::chrono::seconds(5));
		}
		}).detach();
}

auto UniSequence::InitUpdateChckThread(void) -> void
{
	std::thread([&] {
		httplib::Client updateReq(GITHUB_UPDATE);
		updateReq.set_connection_timeout(10, 0);
		LogToES("Start updates check routine.");
		if (update_check_flag_) {
			if (auto result = updateReq.Get(GITHUB_UPDATE_PATH)) {
				json versionInfo = json::parse(result->body);
				if (versionInfo.contains("message")) {
					LogToES("Error occured while checking updates.");
					LogToES(versionInfo["message"]);
					return;
				}
				if (!versionInfo.is_array()) {
					LogToES("Error occured while checking updates.");
					return;
				}
				std::string versionTag = versionInfo[0]["tag_name"];
				std::string versionName = versionInfo[0]["name"];
				std::string publishDate = versionInfo[0]["created_at"];
				if (versionTag != PLUGIN_VER) {
					LogToES(std::format("Update is available! The latest version is: {} {} | Publish date: {}", versionName, versionTag, publishDate));
				}
				// Now, an update prompt will only appear when and only when there is an update, without indicating that this plugin is the latest version
			}
			else {
				LogToES(std::format("Error occured while checking updates - {}", httplib::to_string(result.error())));
			}
		}
		}).detach();
}

auto UniSequence::InitializeLogEnv(void) -> void
{
	remove(LOG_FILE_NAME);
	LogToFile("UniSequence initializing");
	LogToFile("Attempting to register tag item type of \"Sequence\"");
}
auto UniSequence::InitTagItem(void) -> void
{
	RegisterTagItemType(SEQUENCE_TAGITEM_TYPE_CODE_NAME, SEQUENCE_TAGITEM_TYPE_CODE);
	RegisterTagItemFunction(SEQUENCE_TAGFUNC_SWITCH_STATUS, SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE);
	RegisterTagItemFunction(SEQUENCE_TAGFUNC_REORDER_INPUT, SEQUENCE_TAGITEM_FUNC_REORDER);
	RegisterTagItemFunction(SEQUENCE_TAGFUNC_REORDER_SELECT, SEQUENCE_TAGITEM_FUNC_REORDER_SELECT);
}
UniSequence::UniSequence(void) : CPlugIn(
	EuroScopePlugIn::COMPATIBILITY_CODE,
	PLUGIN_NAME, PLUGIN_VER, PLUGIN_AUTHOR, PLUGIN_COPYRIGHT
)
{
	InitializeLogEnv();
	// Registering a Tag Object for EuroScope
	InitTagItem();

	/*init communication thread*/
#ifdef USE_WEBSOCKET
	InitWsThread();
#else
	initDataSyncThread();
#endif // USE_WEBSOCKET
	// update check thread has been detached, so it's safe
	InitUpdateChckThread();

	LogToES("Initialization complete.");
}

UniSequence::~UniSequence(void)
{
	sync_thread_flag_ = false;
	update_check_flag_ = false;
}

auto UniSequence::LogToES(std::string message) -> void
{
	DisplayUserMessage("UniSequence", "system", message.c_str(),
		false, true, true, true, true);
	LogToFile(message);
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
		LogToES("UniSequence Plugin By VATPRC DIVISION.");
		LogToES("Author: Ericple Garrison");
		LogToES("For help and bug report, refer to https://github.com/Ericple/VATPRC-UniSequence");
		return true;
	}
	// Set airports to be listened
	if (CommandMatch(cmd, ".SQA"))
	{
		LogToFile("command \".SQA\" acknowledged.");
		try
		{
			std::unique_lock<std::shared_mutex> airportLock(airport_list_lock_);
			LogToFile("Spliting cmd");
			std::stringstream ss(cmd);
			char delim = ' ';
			std::string item;
			LogToFile("Clearing airport list");
			airport_list_.clear();
			LogToFile("Airport list cleared");
			while (getline(ss, item, delim))
			{
				if (!item.empty() && item[0] != '.')  // the command itself has been skiped here
				{

					LogToFile(std::format("Adding {} to airport list", item));
					airport_list_.push_back(item);
				}
			}
			if (airport_list_.size() > 1) LogToES("Airports saved.");
			if (airport_list_.size() == 1) LogToES("Airport saved.");
			if (airport_list_.size() < 1) LogToES("Airport cleared.");
		}
		catch (std::runtime_error const& e)
		{
			LogToES(std::format("Error: {}", e.what()));
		}
		return true;
	}
	if (CommandMatch(cmd, ".SQP"))
	{
		LogToFile("command \".SQP\" acknowledged.");
		LogToES("Current in list: ");
		std::shared_lock<std::shared_mutex> airportLock(airport_list_lock_);
		LogToES(std::to_string(airport_list_.size()));
		return true;
	}
	if (CommandMatch(cmd, ".SQC"))
	{
		LogToFile("command \".SQC\" acknowledged.");
		try
		{
			LogToFile("Spliting cmd");
			std::stringstream ss(cmd);
			char delim = ' ';
			std::string item;
			LogToFile("Clearing airport list");
			airport_list_.clear();
			LogToFile("Airport list cleared");
			while (getline(ss, item, delim))
			{
				if (!item.empty() && item[0] != '.')  // the command itself has been skiped here
				{
					LogToFile(std::format("Saving logon code: {} to settings", item));
					std::string lowercode = item.c_str();
					transform(lowercode.begin(), lowercode.end(), lowercode.begin(), ::tolower);
					SaveDataToSettings(PLUGIN_SETTING_KEY_LOGON_CODE, PLUGIN_SETTING_DESC_LOGON_CODE, lowercode.c_str());
					LogToFile("Logon code saved.");
				}
			}
			LogToES(MSG_LOGON_CODE_SAVED);
		}
		catch (std::runtime_error const& e)
		{
			LogToES(std::format("Error: {}", e.what()));
		}
		return true;
	}
	return false;
}
auto UniSequence::OnCompileCommand(const char* sCommandLine) -> bool
{
	std::string cmd = sCommandLine;
	std::regex unisRegex(".");
	LogToFile(std::format("Command received: ", cmd));
	transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);
	return CustomCommandHanlder(cmd);
}

auto UniSequence::PatchAircraftStatus(CFlightPlan fp, int status) -> void
{
	std::string cs = fp.GetCallsign();
	LogToFile(std::format("Attempting to patch status of {}", cs));
	auto logon_code_setting = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
	logon_code_ = logon_code_setting != nullptr ? logon_code_setting : "";
	if (!logon_code_.size())
	{
		LogToES(ERR_LOGON_CODE_NULLREF);
		return;
	}
	std::thread patchThread([fp, status, this] {
		json reqBody = {
			{JSON_KEY_CALLSIGN, fp.GetCallsign()},
			{JSON_KEY_STATUS, status}
		};
		httplib::Client patchReq(SERVER_ADDRESS_PRC);
		patchReq.set_connection_timeout(10, 0);
		std::string ap = fp.GetFlightPlanData().GetOrigin();
		if (auto result = patchReq.Patch(std::format("{}{}/status", SERVER_RESTFUL_VER, ap), { {HEADER_LOGON_KEY, logon_code_} }, reqBody.dump().c_str(), "application/json"))
		{
			if (result->status == 200)
			{
				SetQueueFromJson(ap, result->body);
			}
			else
			{
				if (result->status == 403)
				{
					LogToES("Logon code verification failed.");
				}
			}
		}
		else
		{
			LogToES(ERR_CONN_FAILED);
		}
		});
	patchThread.detach();
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
	LogToFile("Function: SEQUENCE_TAG_ITEM_FUNC_REORDER was called");
	OpenPopupEdit(area, SEQUENCE_TAGITEM_FUNC_REORDER_EDITED, "");
}

auto UniSequence::OpenStatusAsignMenu(RECT area, CFlightPlan fp) -> void
{
	LogToFile("Function: SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE was called");
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
	LogToFile("Function: SEQUENCE_TAGITEM_FUNC_REORDER_EDITED was called");
	auto logon_code_setting = GetDataFromSettings(PLUGIN_SETTING_KEY_LOGON_CODE);
	logon_code_ = logon_code_setting != nullptr ? logon_code_setting : "";
	if (!logon_code_.size())
	{
		LogToES(ERR_LOGON_CODE_NULLREF);
		return;
	}
	if (beforeKey == SEQUENCE_TAGFUNC_REORDER_TOPKEY)
	{ // to the moon
		beforeKey = "-1";
	}
	LogToFile("Creating an new thread for reorder request");
	std::thread([beforeKey, fp, this] {
		httplib::Client req(SERVER_ADDRESS_PRC);
		req.set_connection_timeout(10, 0);
		std::string ap = fp.GetFlightPlanData().GetOrigin();
		json reqBody = {
			{JSON_KEY_CALLSIGN, fp.GetCallsign()},
			{JSON_KEY_BEFORE, beforeKey}
		};

		if (auto res = req.Patch(std::format("{}{}/order", SERVER_RESTFUL_VER, ap), { {HEADER_LOGON_KEY, logon_code_} }, reqBody.dump(), "application/json"))
		{
			if (res->status == 200)
			{
				SetQueueFromJson(ap, res->body);
				return;
			}
		}
		else
		{
			LogToES(httplib::to_string(res.error()));
			return;
		}
		}
	).detach();
	LogToFile("reorder thread detached");
}

auto UniSequence::OnFunctionCall(int fId, const char* sItemString, POINT pt, RECT area) -> void
{
	LogToFile(std::format("Function (ID: {}) is called by EuroScope, param: {}", fId, sItemString));
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
		LogToFile("Function: FUNC_SWITCH_TO_WFCR was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_WFCR);
		break;
	case FUNC_SWITCH_TO_CLRD:
		LogToFile("Function: FUNC_SWITCH_TO_CLRD was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_CLRD);
		break;
	case FUNC_SWITCH_TO_WFPU:
		LogToFile("Function: FUNC_SWITCH_TO_WFPU was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_WFPU);
		break;
	case FUNC_SWITCH_TO_PUSH:
		LogToFile("Function: FUNC_SWITCH_TO_PUSH was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_PUSH);
		break;
	case FUNC_SWITCH_TO_WFTX:
		LogToFile("Function: FUNC_SWITCH_TO_WFTX was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_WFTX);
		break;
	case FUNC_SWITCH_TO_TAXI:
		LogToFile("Function: FUNC_SWITCH_TO_TAXI was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_TAXI);
		break;
	case FUNC_SWITCH_TO_WFTO:
		LogToFile("Function: FUNC_SWITCH_TO_WFTO was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_WFTO);
		break;
	case FUNC_SWITCH_TO_TOGA:
		LogToFile("Function: FUNC_SWITCH_TO_TOGA was called");
		PatchAircraftStatus(fp, AIRCRAFT_STATUS_TOGA);
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
	std::unique_lock<std::shared_mutex> airportLock(airport_list_lock_);
	if (std::find_if(airport_list_.cbegin(), airport_list_.cend(), is_same_airport) == airport_list_.cend()) {
		LogToFile(std::format("Airport {} is not in the list.", dep_airport));
		airport_list_.push_back(dep_airport);
		LogToFile(std::format("Airport {} added.", dep_airport));
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
