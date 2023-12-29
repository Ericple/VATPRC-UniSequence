#pragma once

using namespace EuroScopePlugIn;

#ifndef COPYRIGHTS
#define AUTHOR "Ericple Garrison"
#define GITHUB_LINK "https://github.com/Ericple/VATPRC-UniSequence"
#define GITHUB_UPDATE "https://api.github.com"
#define GITHUB_UPDATE_PATH "/repos/Ericple/VATPRC-UniSequence/releases"
#define SERVER_ADDRESS_PRC "https://q.vatprc.net"
#define WS_ADDRESS_PRC "ws://q.vatprc.net"
#define SERVER_ADDRESS_WITH_PORT "https://q.vatprc.net:443"
#define SERVER_RESTFUL_VER "/v1/"
#define DIVISION "VATPRC"
#define PLUGIN_NAME "UniSequence"
#define PLUGIN_VER "v2.0.4"
#define PLUGIN_AUTHOR "Ericple Garrison"
#define PLUGIN_COPYRIGHT "AGPL-3.0 license"
#endif

#ifndef Code
#define SEQUENCE_TAGITEM_TYPE_CODE 35
#define SEQUENCE_TAGITEM_TYPE_CODE_NAME "Sequence / Status"
#define SEQUENCE_TAGITEM_FUNC_SWITCH_STATUS_CODE 50
#define SEQUENCE_TAGFUNC_SWITCH_STATUS "Status Popup List"
#define SEQUENCE_TAGFUNC_REORDER_TOPKEY "To the moon"
#define SEQUENCE_TAGITEM_FUNC_REORDER 82
#define SEQUENCE_TAGFUNC_REORDER_INPUT "Reorder Input"
#define SEQUENCE_TAGITEM_FUNC_REORDER_EDITED 94
#define SEQUENCE_TAGITEM_FUNC_REORDER_SELECT 188
#define SEQUENCE_TAGFUNC_REORDER_SELECT "Reorder List (Recommend!)"
#define FUNC_SWITCH_TO_WFCR 84
#define FUNC_SWITCH_TO_CLRD 86
#define FUNC_SWITCH_TO_WFPU 93
#define FUNC_SWITCH_TO_PUSH 99
#define FUNC_SWITCH_TO_WFTX 102
#define FUNC_SWITCH_TO_TAXI 106
#define FUNC_SWITCH_TO_WFTO 110
#define FUNC_SWITCH_TO_TOGA 114
#endif // !Code

#ifndef LIMITATIONS
#define MAXIMUM_AIRPORT_LIST_COUNT 100
#endif // !LIMITATIONS

#ifndef AIRCRAFT_STATUS
#define STATUS_LIST_TITLE "Select Status"
#define STATUS_TEXT_PLACE_HOLDER "________"
#define AIRCRAFT_STATUS_NULL 999 // EMPTY STATUS
#define STATUS_TEXT_NULL "??-----"
#define AIRCRAFT_STATUS_WFCR 70 // WAITING FOR CLEARANCE
#define STATUS_TEXT_WFCR "-CLRD"
#define STATUS_DESC_WFCR "Waiting for clearance"
#define AIRCRAFT_STATUS_CLRD 60 // CLEARANCE GOT
#define STATUS_TEXT_CLRD "+CLRD"
#define STATUS_DESC_CLRD "Clearance granted"
#define AIRCRAFT_STATUS_WFPU 50 // WATING FOR PUSH
#define STATUS_TEXT_WFPU "-PUSH"
#define STATUS_DESC_WFPU "Waiting for push"
#define AIRCRAFT_STATUS_PUSH 40 // PUSHING BACK
#define STATUS_TEXT_PUSH "+PUSH"
#define STATUS_DESC_PUSH "Pushing back"
#define AIRCRAFT_STATUS_WFTX 30 // WATING FOR TAXI
#define STATUS_TEXT_WFTX "-TAXI"
#define STATUS_DESC_WFTX "Waiting for taxi"
#define AIRCRAFT_STATUS_TAXI 20 // TAXI TO RWY
#define STATUS_TEXT_TAXI "+TAXI"
#define STATUS_DESC_TAXI "Taxiing"
#define AIRCRAFT_STATUS_WFTO 10 // WAITING FOR TAKE OFF
#define STATUS_TEXT_WFTO "-TKOF"
#define STATUS_DESC_WFTO "Waiting for take off"
#define AIRCRAFT_STATUS_TOGA 0 // TAKE OFF
#define STATUS_TEXT_TOGA "+TKOF"
#define STATUS_DESC_TKOF "Taking off"
#define STATUS_COLOR_WAIT RGB(255, 255, 0)
#define STATUS_COLOR_IN_PROGRESS RGB(0, 204,0)
#define STATUS_TEXT_FORMAT_STRING "%02d%s"
#endif // !AIRCRAFT_STATUS

#ifndef PLUGIN_SETTING_KEYS
#define PLUGIN_SETTING_KEY_LOGON_CODE "logonstr"
#define PLUGIN_SETTING_DESC_LOGON_CODE "Your logon code for unisequence plugin to establish connection with vatprc server."
#endif // !PLUGIN_SETTING_KEYS

#ifndef MESSAGE
#define ERR_LOGON_CODE_NULLREF "You haven't set your logon code, please set your code first before using this plugin by enter \".sqc <your code here>\"."
#define ERR_CONN_FAILED "Failed to connect to queue server."
#define MSG_LOGON_CODE_SAVED "Your logon code has been saved."
#endif // !MESSAGE

#ifndef REQUEST_RELATED
#define JSON_KEY_CALLSIGN "callsign"
#define JSON_KEY_STATUS "status"
#define JSON_KEY_BEFORE "before"
#define DEFAULT_LOGON_CODE "testlogon"
#define HEADER_LOGON_KEY "Authorization"
#endif // !REQUEST_RELATED

#ifndef LOGGER_RELATED
#define LOG_FILE_NAME "unilog.log"
#endif // !LOGGER_RELATED

//===========================
// Aircraft instance
//===========================
typedef struct SequenceNode {
	std::string callsign, origin;
	int status, sequenceNumber;
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
	std::mutex queue_cache_lock_, log_lock_;
	std::vector<std::string> airport_list_;
	auto LogToES(std::string) -> void;
	auto LogToFile(std::string) -> void;
	auto GetManagedAircraft(CFlightPlan) -> SeqNode*;
	auto PatchAircraftStatus(CFlightPlan, int) -> void;
	virtual auto OnCompileCommand(const char*) -> bool;
	virtual auto OnGetTagItem(CFlightPlan, CRadarTarget, 
		int, int, char[16], int*, COLORREF*, double*) -> void;
	virtual auto OnFunctionCall(int, const char*, POINT, RECT) -> void;
	auto SetQueueFromJson(const std::string&, const std::string&) -> void;
private:
	int timer_interval_ = 5;
	const char* logon_code_;
	nlohmann::json queue_caches_;
	bool sync_thread_flag_ = true;
	bool update_check_flag_ = true;
	std::thread* update_check_thread_;
	auto InitTagItem(void) -> void;
	auto InitializeLogEnv(void) -> void;
	auto InitUpdateChckThread(void) -> void;
	auto ReorderAircraftByEdit(RECT) -> void;
	auto CustomCommandHanlder(std::string) -> bool;
	auto OpenStatusAsignMenu(RECT, CFlightPlan) -> void;
	auto AddAirportIfNotExist(const std::string&) -> void;
	auto CommandMatch(const std::string&, const char*) -> bool;
	auto IsTagItemValid(int, CFlightPlan, CRadarTarget) -> bool;
	auto ReorderAircraftBySelect(SeqNode*, RECT, const std::string&) -> void;
	auto ReorderAircraftEditHandler(SeqNode*, CFlightPlan, const char*) -> void;
#ifdef USE_WEBSOCKET
	std::thread* ws_sync_thread_;
	auto InitWsThread(void) -> void;
#else
	thread* dataSyncThread;
	auto initDataSyncThread(void) -> void;
#endif
};

