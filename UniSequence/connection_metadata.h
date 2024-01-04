#pragma once
#include "UniSequence.h"
#ifdef USE_WEBSOCKET

typedef websocketpp::client<websocketpp::config::asio_client> client;

class connection_metadata
{
public:
	typedef websocketpp::lib::shared_ptr<connection_metadata> ptr;
	connection_metadata(int, websocketpp::connection_hdl, std::string, UniSequence*, std::string);
	void on_open(client*, websocketpp::connection_hdl);
	void on_fail(client*, websocketpp::connection_hdl);
	void on_close(client*, websocketpp::connection_hdl);
	void on_message(websocketpp::connection_hdl, client::message_ptr);
	websocketpp::connection_hdl get_hdl() const;
	int get_id() const;
	std::string get_status() const;
	std::ofstream wsDataStream;
private:
	int m_id;
	websocketpp::connection_hdl m_hdl;
	std::string m_status;
	std::string m_uri;
	std::string m_server;
	std::string m_error_reason;
	UniSequence* uniptr;
	std::string icao;
};
#endif
