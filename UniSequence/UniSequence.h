#pragma once

using namespace EuroScopePlugIn;

#ifndef COPYRIGHTS
constexpr auto AUTHOR = "Ericple Garrison";
constexpr auto GITHUB_LINK = "https://github.com/Ericple/VATPRC-UniSequence";
constexpr auto GITHUB_UPDATE = "https://api.github.com";
constexpr auto GITHUB_UPDATE_PATH = "/repos/Ericple/VATPRC-UniSequence/releases";
constexpr auto SERVER_ADDRESS_PRC = "https://q.vatprc.net";
constexpr auto WS_ADDRESS_PRC = "ws://q.vatprc.net";
constexpr auto SERVER_ADDRESS_WITH_PORT = "https://q.vatprc.net:443";
constexpr auto SERVER_RESTFUL_VER = "/v1/";
constexpr auto DIVISION = "VATPRC";
constexpr auto PLUGIN_NAME = "UniSequence";
constexpr auto PLUGIN_VER = "v2.0.5-nightly";
constexpr auto PLUGIN_AUTHOR = "Ericple Garrison";
constexpr auto PLUGIN_COPYRIGHT = "AGPL-3.0 license";
#endif

#ifndef Code
constexpr auto SEQUENCE_TAGITEM_TYPE_CODE = 35;
constexpr auto SEQUENCE_TAGITEM_TYPE_CODE_NAME = "Sequence / Status";
constexpr auto SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE = 50;
constexpr auto SEQUENCE_TAGFUNC_SWITCH_STATUS = "Status Popup List";
constexpr auto SEQUENCE_TAGFUNC_REORDER_TOPKEY = "To the moon";
constexpr auto SEQUENCE_TAGITEM_FUNC_REORDER = 82;
constexpr auto SEQUENCE_TAGFUNC_REORDER_INPUT = "Reorder Input";
constexpr auto SEQUENCE_TAGITEM_FUNC_REORDER_EDITED = 94;
constexpr auto SEQUENCE_TAGITEM_FUNC_REORDER_SELECT = 188;
constexpr auto SEQUENCE_TAGFUNC_REORDER_SELECT = "Reorder List (Recommended)";
constexpr auto FUNC_SWITCH_TO_WFCR = 84;
constexpr auto FUNC_SWITCH_TO_CLRD = 86;
constexpr auto FUNC_SWITCH_TO_WFPU = 93;
constexpr auto FUNC_SWITCH_TO_PUSH = 99;
constexpr auto FUNC_SWITCH_TO_WFTX = 102;
constexpr auto FUNC_SWITCH_TO_TAXI = 106;
constexpr auto FUNC_SWITCH_TO_WFTO = 110;
constexpr auto FUNC_SWITCH_TO_TOGA = 114;
#endif // !Code

#ifndef LIMITATIONS
constexpr auto MAXIMUM_AIRPORT_LIST_COUNT = 100;
#endif // !LIMITATIONS

#ifndef AIRCRAFT_STATUS
constexpr auto STATUS_LIST_TITLE = "Select Status";
constexpr auto STATUS_TEXT_PLACE_HOLDER = "________";
constexpr auto AIRCRAFT_STATUS_NULL = 999; // EMPTY STATUS
constexpr auto STATUS_TEXT_NULL = "??-----";
constexpr auto AIRCRAFT_STATUS_WFCR = 70; // WAITING FOR CLEARANCE
constexpr auto STATUS_TEXT_WFCR = "-CLRD";
constexpr auto STATUS_DESC_WFCR = "Waiting for clearance";
constexpr auto AIRCRAFT_STATUS_CLRD = 60; // CLEARANCE GOT
constexpr auto STATUS_TEXT_CLRD = "+CLRD";
constexpr auto STATUS_DESC_CLRD = "Clearance granted";
constexpr auto AIRCRAFT_STATUS_WFPU = 50; // WATING FOR PUSH
constexpr auto STATUS_TEXT_WFPU = "-PUSH";
constexpr auto STATUS_DESC_WFPU = "Waiting for push";
constexpr auto AIRCRAFT_STATUS_PUSH = 40; // PUSHING BACK
constexpr auto STATUS_TEXT_PUSH = "+PUSH";
constexpr auto STATUS_DESC_PUSH = "Pushing back";
constexpr auto AIRCRAFT_STATUS_WFTX = 30; // WATING FOR TAXI
constexpr auto STATUS_TEXT_WFTX = "-TAXI";
constexpr auto STATUS_DESC_WFTX = "Waiting for taxi";
constexpr auto AIRCRAFT_STATUS_TAXI = 20; // TAXI TO RWY
constexpr auto STATUS_TEXT_TAXI = "+TAXI";
constexpr auto STATUS_DESC_TAXI = "Taxiing";
constexpr auto AIRCRAFT_STATUS_WFTO = 10; // WAITING FOR TAKE OFF
constexpr auto STATUS_TEXT_WFTO = "-TKOF";
constexpr auto STATUS_DESC_WFTO = "Waiting for take off";
constexpr auto AIRCRAFT_STATUS_TOGA = 0; // TAKE OFF
constexpr auto STATUS_TEXT_TOGA = "+TKOF";
constexpr auto STATUS_DESC_TKOF = "Taking off";
constexpr auto STATUS_COLOR_WAIT = RGB(255, 255, 0);
constexpr auto STATUS_COLOR_IN_PROGRESS = RGB(0, 204, 0);
constexpr auto STATUS_TEXT_FORMAT_STRING = "%02d%s";
#endif // !AIRCRAFT_STATUS

#ifndef PLUGIN_SETTING_KEYS
constexpr auto PLUGIN_SETTING_KEY_LOGON_CODE = "logonstr";
constexpr auto PLUGIN_SETTING_DESC_LOGON_CODE = "Your logon code for unisequence plugin to establish connection with vatprc server.";
#endif // !PLUGIN_SETTING_KEYS

#ifndef MESSAGE
constexpr auto ERR_LOGON_CODE_NULLREF = "You haven't set your logon code, please set your code first before using this plugin by enter \".sqc <your code here>\".";
constexpr auto ERR_CONN_FAILED = "Failed to connect to queue server.";
constexpr auto MSG_LOGON_CODE_SAVED = "Your logon code has been saved.";
#endif // !MESSAGE

#ifndef REQUEST_RELATED
constexpr auto JSON_KEY_CALLSIGN = "callsign";
constexpr auto JSON_KEY_STATUS = "status";
constexpr auto JSON_KEY_BEFORE = "before";
constexpr auto DEFAULT_LOGON_CODE = "testlogon";
constexpr auto HEADER_LOGON_KEY = "Authorization";
#endif // !REQUEST_RELATED

#ifndef LOGGER_RELATED
constexpr auto LOG_FILE_NAME = "unilog.log";
#endif // !LOGGER_RELATED

//===========================
// Aircraft instance
//===========================
typedef struct SequenceNode {
	std::string callsign;
	std::string origin;
	int status;
	int sequenceNumber;
	bool seqNumUpdated;
} SeqNode;

//============================
// Socket instance of an airport
//============================
typedef struct AirportSocket {
	std::string icao;
	int socketId;
} ASocket;

class UniSequence : public CPlugIn
{
public:
	UniSequence();
	~UniSequence();

	std::ofstream log_stream_;
	std::vector<ASocket> socket_list_;
	std::vector<std::string> airport_list_;
	std::mutex log_lock_;
	std::shared_mutex queue_cache_lock_, airport_list_lock_, socket_list_lock_;

	auto LogToES(std::string) -> void;
	auto LogToFile(std::string) -> void;
	auto GetManagedAircraft(CFlightPlan) -> std::shared_ptr<SeqNode>;
	auto PatchAircraftStatus(CFlightPlan, int) -> void;
	virtual auto OnCompileCommand(const char*) -> bool;
	virtual auto OnGetTagItem(CFlightPlan, CRadarTarget,
		int, int, char[16], int*, COLORREF*, double*) -> void;
	virtual auto OnFunctionCall(int, const char*, POINT, RECT) -> void;
	auto SetQueueFromJson(const std::string&, const std::string&) -> void;

private:
	int timer_interval_ = 5;
	std::string logon_code_;
	nlohmann::json queue_caches_; // has a shared_lock
	std::atomic_bool sync_thread_flag_ = true;
	std::atomic_bool update_check_flag_ = true;

	auto InitTagItem(void) -> void;
	auto InitializeLogEnv(void) -> void;
	auto InitUpdateChckThread(void) -> void;
	auto ReorderAircraftByEdit(RECT) -> void;
	auto CustomCommandHanlder(std::string) -> bool;
	auto OpenStatusAsignMenu(RECT, CFlightPlan) -> void;
	auto AddAirportIfNotExist(const std::string&) -> void;
	auto CommandMatch(const std::string&, const char*) -> bool;
	auto ReorderAircraftBySelect(std::shared_ptr<SeqNode>, RECT, const std::string&) -> void;
	auto ReorderAircraftEditHandler(std::shared_ptr<SeqNode>, CFlightPlan, const char*) -> void;
	auto InitWsThread(void) -> void;

};

