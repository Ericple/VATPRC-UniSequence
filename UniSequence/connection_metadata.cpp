#include "pch.h"
#ifdef USE_WEBSOCKET
#include "connection_metadata.h"
#include "UniSequence.h"
connection_metadata::connection_metadata(int id, websocketpp::connection_hdl hdl, string uri, UniSequence* pUniSeq, string apcode)
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
		string resBody = msg->get_payload().c_str();
		uniptr->setQueueJson(icao, "{\"data\":" + resBody + "}");
		//uniptr->Update_s_queueCache("{\"data\":"+resBody+"}");
		/*nlohmann::json ws_data_json = nlohmann::json::parse(resBody);
		int seqNum = 1;
		try
		{
			for (auto& seqObj : ws_data_json)
			{
				string cs = seqObj[JSON_KEY_CALLSIGN];
				int status = seqObj[JSON_KEY_STATUS];
				uniptr->SyncSeq(cs, status);
				uniptr->SyncSeqNum(cs, seqNum);
				seqNum++;
			}
			uniptr->ClearUpdateFlag(icao);
		}
		catch (exception const& e)
		{
			uniptr->Messager(e.what());
		}*/
	}
}

websocketpp::connection_hdl connection_metadata::get_hdl() const {
	return m_hdl;
}

int connection_metadata::get_id() const {
	return m_id;
}

string connection_metadata::get_status() const {
	return m_status;
}

#endif