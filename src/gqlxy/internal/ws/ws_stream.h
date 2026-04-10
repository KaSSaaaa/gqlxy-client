#pragma once

#include "ws_stream_base.h"

#include <boost/beast/core/tcp_stream.hpp>

namespace gqlxy::internal {

class WsStream : public WsStreamBase<boost::beast::tcp_stream> {
public:
    WsStream(const ParsedUrl& url, const std::map<std::string, std::string>& headers);
};

}
