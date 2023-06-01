#pragma once
#include "connection_metadata.h"
#include "UniSequence.h"
#ifdef USE_WEBSOCKET
using namespace std;

class websocket_endpoint
{
public:
	websocket_endpoint(UniSequence*);
	~websocket_endpoint();
	int connect(string const&, string);
	void close(int, websocketpp::close::status::value, string);
	connection_metadata::ptr get_metadata(int) const;
private:
	typedef std::map<int, connection_metadata::ptr> con_list;

	client m_endpoint;
	websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;

	con_list m_connection_list;
	int m_next_id;
	UniSequence* uniptr;
};

#endif