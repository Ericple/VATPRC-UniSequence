#include "pch.h"
#include "websocket_endpoint.h"
#include "connection_metadata.h"
#ifdef USE_WEBSOCKET
websocket_endpoint::websocket_endpoint(UniSequence* ptr) : m_next_id(0) {
    m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
    m_endpoint.clear_error_channels(websocketpp::log::elevel::all);

    m_endpoint.init_asio();
    m_endpoint.start_perpetual();

    m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&client::run, &m_endpoint);
    uniptr = ptr;
}

websocket_endpoint::~websocket_endpoint() {
    m_endpoint.stop_perpetual();

    for (con_list::const_iterator it = m_connection_list.begin(); it != m_connection_list.end(); ++it) {
        if (it->second->get_status() != "Open") {
            // Only close open connections
            continue;
        }

        websocketpp::lib::error_code ec;
        m_endpoint.close(it->second->get_hdl(), websocketpp::close::status::going_away, "", ec);
    }

    m_thread->join();
}

int websocket_endpoint::connect(std::string const& uri) {
    websocketpp::lib::error_code ec;

    client::connection_ptr con = m_endpoint.get_connection(uri, ec);

    int new_id = m_next_id++;
    connection_metadata::ptr metadata_ptr = websocketpp::lib::make_shared<connection_metadata>(new_id, con->get_handle(), uri, uniptr);
    m_connection_list[new_id] = metadata_ptr;

    con->set_open_handler(websocketpp::lib::bind(
        &connection_metadata::on_open,
        metadata_ptr,
        &m_endpoint,
        websocketpp::lib::placeholders::_1
    ));
    con->set_fail_handler(websocketpp::lib::bind(
        &connection_metadata::on_fail,
        metadata_ptr,
        &m_endpoint,
        websocketpp::lib::placeholders::_1
    ));
    con->set_close_handler(websocketpp::lib::bind(
        &connection_metadata::on_close,
        metadata_ptr,
        &m_endpoint,
        websocketpp::lib::placeholders::_1
    ));
    con->set_message_handler(websocketpp::lib::bind(
        &connection_metadata::on_message,
        metadata_ptr,
        websocketpp::lib::placeholders::_1,
        websocketpp::lib::placeholders::_2
    ));

    m_endpoint.connect(con);

    return new_id;
}

void websocket_endpoint::close(int id, websocketpp::close::status::value code, std::string reason) {
    websocketpp::lib::error_code ec;

    con_list::iterator metadata_it = m_connection_list.find(id);

    m_endpoint.close(metadata_it->second->get_hdl(), code, reason, ec);
}

connection_metadata::ptr websocket_endpoint::get_metadata(int id) const {
    con_list::const_iterator metadata_it = m_connection_list.find(id);
    if (metadata_it == m_connection_list.end()) {
        return connection_metadata::ptr();
    }
    else {
        return metadata_it->second;
    }
}
#endif