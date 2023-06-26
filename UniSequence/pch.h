#ifndef PCH_H
#define PCH_H

#define CPPHTTPLIB_OPENSSL_SUPPORT
#define PATCH_WITH_LOGON_CODE
#define USE_WEBSOCKET

#include "framework.h"
#include <iostream>
#ifdef USE_WEBSOCKET
#include "websocketpp/config/asio_no_tls_client.hpp"
#include "websocketpp/client.hpp"
#include "websocketpp/common/thread.hpp"
#include "websocketpp/common/memory.hpp"
typedef websocketpp::client<websocketpp::config::asio_client> client;
#endif
#include <mutex>
#include <locale>
#include <codecvt>
#include <cstdlib>
#include <string>
#include "include/httplib.h"
#include <Windows.h>
#include "include/EuroScopePlugIn.h"
#include <fstream>
#include <time.h>
#include <algorithm>
#include "include/json.hpp"
#include <future>
#include <map>
#include <list>
#include <vector>
#include <thread>
#include <regex>



#endif //PCH_H
