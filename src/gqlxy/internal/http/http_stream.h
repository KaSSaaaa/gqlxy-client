#pragma once

#include "http_stream_base.h"

#include <boost/beast/core/tcp_stream.hpp>

namespace gqlxy::internal {

class HttpStream : public HttpStreamBase<boost::beast::tcp_stream> {
public:
    HttpStream(const boost::asio::any_io_executor& ex);

    boost::asio::awaitable<void> Connect(const std::string& host, const std::string& port) override;
    boost::asio::awaitable<void> Shutdown() override;
};

}
