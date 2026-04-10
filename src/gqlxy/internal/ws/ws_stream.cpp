#include "ws_stream.h"

#include <gqlxy/internal/asio_context.h>

using namespace std;
using namespace gqlxy::internal;
using namespace boost::asio::ssl;
using namespace boost::beast;

WsStream::WsStream(const ParsedUrl& url, const map<string, string>& headers)
    : WsStreamBase(url, headers, websocket::stream<tcp_stream>(AsioContext::Get())) {}