#include "pch.h"
#ifdef USE_WEBSOCKET
#include "connection_metadata.h"
#include "UniSequence.h"
connection_metadata::connection_metadata(int id, websocketpp::connection_hdl hdl, std::string uri, UniSequence* pUniSeq, std::string apcode)
	: m_id(id), m_hdl(hdl), m_status("Connecting"), m_uri(uri), m_server("N/A")
{
	uniptr = pUniSeq;
	icao = apcode;
}

void connection_metadata::on_open(client* c, websocketpp::connection_hdl hdl)
{
	m_status = "Open";
	client::connection_ptr con = c->get_con_from_hdl(hdl);
	m_server = con->get_response_header("Server");
}

void connection_metadata::on_fail(client* c, websocketpp::connection_hdl hdl)
{
	m_status = "Failed";
	client::connection_ptr con = c->get_con_from_hdl(hdl);
	m_server = con->get_response_header("Server");
	m_error_reason = con->get_ec().message();
}

void connection_metadata::on_close(client* c, websocketpp::connection_hdl hdl)
{
	m_status = "Closed";
	client::connection_ptr con = c->get_con_from_hdl(hdl);
	std::stringstream s;
	m_error_reason = s.str();
}

void connection_metadata::on_message(websocketpp::connection_hdl, client::message_ptr msg) {
	if (msg->get_opcode() == websocketpp::frame::opcode::text)
	{
		std::string resBody = msg->get_payload().c_str();
		uniptr->SetQueueFromJson(icao, "{\"data\":" + resBody + "}");
	}
}

websocketpp::connection_hdl connection_metadata::get_hdl() const {
	return m_hdl;
}

int connection_metadata::get_id() const {
	return m_id;
}

std::string connection_metadata::get_status() const {
	return m_status;
}

#endif