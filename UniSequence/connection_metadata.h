#pragma once
#include "UniSequence.h"
#ifdef USE_WEBSOCKET
using namespace std;

class connection_metadata
{
public:
	typedef websocketpp::lib::shared_ptr<connection_metadata> ptr;
	connection_metadata(int, websocketpp::connection_hdl, string, UniSequence*, string);
	void on_open(client*, websocketpp::connection_hdl);
	void on_fail(client*, websocketpp::connection_hdl);
	void on_close(client*, websocketpp::connection_hdl);
	void on_message(websocketpp::connection_hdl, client::message_ptr);
	websocketpp::connection_hdl get_hdl() const;
	int get_id() const;
	string get_status() const;
	ofstream wsDataStream;
private:
	int m_id;
	websocketpp::connection_hdl m_hdl;
	string m_status;
	string m_uri;
	string m_server;
	string m_error_reason;
	UniSequence* uniptr;
	string icao;
};
#endif
