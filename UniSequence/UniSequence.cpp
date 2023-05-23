#include "pch.h"
#include "UniSequence.h"
#include "queueItem.h"

using namespace std;
using nlohmann::json;

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

	return false;
}

string UniSequence::FetchQueue(string airport)
{
	httplib::Client client(SERVER_ADDRESS_PRC, SERVER_PORT_PRC);
	client.set_connection_timeout(5, 0);
	if (auto res = client.Get("/v1/" + airport + "/queue"))
	{
		if (res->status != 200) return "HTTPERR:" + httplib::to_string(res.error());
		json queue = json::parse(FetchQueue(res->body));
		Messager("Sequence of " + airport + " fetched.");
		for (auto& el : queue["data"])
		{
			if (!el["callsign"].is_null() && !el["status"].is_null())
			{
				string callsign = el["callsign"];
				int status = el["status"];
			}
		}
		return res->body;
	}
	else
	{
		return "Get failed: " + httplib::to_string(res.error());
	}
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
	sprintf_s(sItemString, 5, "TEST");
	// Every time the tag refresh, we need to fetch seq data from network.

	return;
}

void UniSequence::OnTimer(int interval)
{
	if (!(interval % timerInterval))
	{
		if (airportList.size() == 0) return;
		for (const string airport : airportList)
		{
			try
			{
				string result = FetchQueue(airport);
				Messager(result);
				/*if (json queue = json::parse(FetchQueue(airport)))
				{
					for (auto& el : queue["data"])
					{
						if (!el["callsign"].is_null() && !el["status"].is_null())
						{
							string callsign = el["callsign"];
							int status = el["status"];
						}
					}
					Messager("Sequence of " + airport + " fetched.");
				}
				else
				{
					Messager(result);
				}*/
			}
			catch (exception const& e)
			{
				Messager("Error occured during itering queue json");
				Messager(e.what());
				continue;
			}
		}
	}
}