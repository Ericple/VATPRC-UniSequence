#include "pch.h"
#include "UniSequence.h"
#include "queueItem.h"

using namespace std;
using nlohmann::json;

bool globalIsOperating = false;

UniSequence::UniSequence(void)
{
	RegisterTagItemType("Sequence", SEQUENCE_TAGITEM_TYPE_CODE);
	RegisterTagItemFunction("Sequence Popup List", SEQUENCE_TAGITEM_FUNC_CODE);
	//airportList.push_back("ZGGG");
	Messager("UniSequence Plugin Loaded.");
}

UniSequence::~UniSequence(void){}

void UniSequence::Messager(string message)
{
	DisplayUserMessage("Message", "UniSequence", message.c_str(),
		false, true, true, false, true);
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
		Messager("For help and bug report, refer to https://github.com/Ericple/uni-sequence");
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
	// For debug use
	if (cmd.substr(0, 4) == ".SQS")
	{
		char i[10];
		_itoa_s(Sequence.size(), i, 10);
		Messager("Current in sequence: ");
		Messager(i);
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

	return false;
}

static void FetchQueueS(string airport, UniSequence* instance)
{
	if (!globalIsOperating)
	{
		globalIsOperating = true;
		httplib::Client client(SERVER_ADDRESS_PRC, SERVER_PORT_PRC);
		client.set_connection_timeout(5, 0);
		if (auto res = client.Get(SERVER_RESTFUL_VER + airport + "/queue"))
		{
			if (res->status != 200) {
				OutputDebugStringA("Unexpected result status, fetch failed\n");
				return;
			}
			json queue = json::parse(res->body);
			instance->Messager("Sequence of " + airport + " fetched.");
			for (auto& el : queue["data"])
			{
				if (!el["callsign"].is_null() && !el["status"].is_null())
				{
					string callsign = el["callsign"];
					int status = el["status"];
					instance->SyncSeq(callsign, status);
				}
			}
		}
		else
		{
			OutputDebugStringA(httplib::to_string(res.error()).c_str());
		}
		globalIsOperating = false;
	}
}
void UniSequence::FetchQueue(string airport)
{
	httplib::Client client(SERVER_ADDRESS_PRC, SERVER_PORT_PRC);
	client.set_connection_timeout(5, 0);
	if (auto res = client.Get(SERVER_RESTFUL_VER + airport + "/queue"))
	{
		if (res->status != 200) {
			OutputDebugStringA("Unexpected result status, fetch failed\n");
			return;
		}
		json queue = json::parse(res->body);
		Messager("Sequence of " + airport + " fetched.");
		for (auto& el : queue["data"])
		{
			if (!el["callsign"].is_null() && !el["status"].is_null())
			{
				string callsign = el["callsign"];
				int status = el["status"];
				SyncSeq(callsign, status);
			}
		}
	}
	else
	{
		OutputDebugStringA(httplib::to_string(res.error()).c_str());
	}
}

void UniSequence::SyncQueue(vector<string> airports)
{
	thread syncThread([&] {
		OutputDebugStringA("Attempting to fetch queue of airport\n");
		for (auto itr = airports.begin(); itr != airports.end(); itr++)
		{
			try
			{
				FetchQueueS(*itr, this);
			}
			catch (const std::exception& e)
			{
				OutputDebugStringA(e.what());
			}
		}
		});
	syncThread.detach();
	
}

void UniSequence::FetchQueueSocket()
{
	
}

void UniSequence::OnGetTagItem(CFlightPlan fp, CRadarTarget rt, int itemCode, int tagData,
	char sItemString[16], int* pColorCode, COLORREF* pRGB, double* pFontSize)
{
	// check if this tag is valid
	if (!fp.IsValid() || !rt.IsValid()) return;
	// check if item code is what we need to handle
	if (itemCode != SEQUENCE_TAGITEM_TYPE_CODE) return;
	switch (itemCode)
	{
	case SEQUENCE_TAGITEM_TYPE_CODE:
		int status = GetSeq(fp.GetCallsign());
		switch (status)
		{
		case AIRCRAFT_STATUS_NULL:
			sprintf_s(sItemString, 5, "NULL");
			break;
		case AIRCRAFT_STATUS_WFCR:
			sprintf_s(sItemString, 5, "WFCR");
			break;
		case AIRCRAFT_STATUS_CLRD:
			sprintf_s(sItemString, 5, "CLRD");
			break;
		case AIRCRAFT_STATUS_WFPU:
			sprintf_s(sItemString, 5, "WFPU");
			break;
		case AIRCRAFT_STATUS_PUSH:
			sprintf_s(sItemString, 5, "PUSH");
			break;
		case AIRCRAFT_STATUS_WFTX:
			sprintf_s(sItemString, 5, "WFTX");
		case AIRCRAFT_STATUS_TAXI:
			sprintf_s(sItemString, 5, "TAXI");
			break;
		case AIRCRAFT_STATUS_WFTO:
			sprintf_s(sItemString, 5, "WFTO");
			break;
		case AIRCRAFT_STATUS_TOGA:
			sprintf_s(sItemString, 5, "TOGA");
			break;
		default: // Get status of -1, represent that it is not in sequence.
			AddToSeq(fp);
			break;
		}
		break;
	}
	
	// Every time the tag refresh, we need to fetch seq data from network.

	return;
}

void UniSequence::OnTimer(int interval)
{
	if (!(interval % timerInterval))
	{
		if (airportList.size() == 0) return;
		Messager("Refreshing queue...");
		SyncQueue(airportList);
	}
}