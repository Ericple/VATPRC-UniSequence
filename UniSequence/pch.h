#ifndef PCH_H
#define PCH_H

#define CPPHTTPLIB_OPENSSL_SUPPORT
#define PATCH_WITH_LOGON_CODE
#define USE_WEBSOCKET

#include "framework.h"

#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
#pragma comment (lib, "User32.lib") // to resolve libcrypto.lib(libcrypto-lib-cryptlib.obj) error LNK2019
#endif

#ifdef USE_WEBSOCKET
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>
#endif

// external deps
#include <EuroScopePlugIn.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

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
#include <set>
#include <map>
#include <vector>
#include <queue>
// others
#include <algorithm>

#endif //PCH_H
