#pragma once

#include <boost/beast/core/tcp_stream.hpp>
#include <gqlxy/client/internal/ws/ws_stream_base.h>
#include <gqlxy/client/link.h>

namespace gqlxy::internal {

class WsStream : public WsStreamBase<boost::beast::tcp_stream> {
public:
    WsStream(const Url& url, const Headers& headers);
};

}
