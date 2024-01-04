#ifndef PCH_H
#define PCH_H

#define CPPHTTPLIB_OPENSSL_SUPPORT
#define PATCH_WITH_LOGON_CODE
#define USE_WEBSOCKET

#include "framework.h"

#ifdef USE_WEBSOCKET
#include "websocketpp/config/asio_no_tls_client.hpp"
#include "websocketpp/client.hpp"
#include "websocketpp/common/thread.hpp"
#include "websocketpp/common/memory.hpp"
#endif

// external deps
#include "include/EuroScopePlugIn.h"
#include "include/httplib.h"
#include "include/json.hpp"

// file
#include <fstream>
// string
#include <string>
#include <regex>
// threading
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <atomic>
// container
#include <map>
#include <vector>
// others
#include <algorithm>



#endif //PCH_H
