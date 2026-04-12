#pragma once

#include "i_http_stream.h"

#include <boost/beast/core/tcp_stream.hpp>

namespace gqlxy::internal {

class HttpStream : public IHttpStream {
public:
    HttpStream(const boost::asio::any_io_executor& ex);

    boost::asio::awaitable<void> Connect(const std::string& host, const std::string& port) override;
    boost::asio::awaitable<void> Write(const boost::beast::http::request<boost::beast::http::string_body>& req) override;
    boost::asio::awaitable<void> ReadHeader(boost::beast::flat_buffer& buf, boost::beast::http::response_parser<boost::beast::http::string_body>& parser) override;
    boost::asio::awaitable<bool> ReadBodyChunk(boost::beast::flat_buffer& buf, boost::beast::http::response_parser<boost::beast::http::string_body>& parser) override;
    boost::asio::awaitable<void> Shutdown() override;

private:
    boost::beast::tcp_stream _stream;
};

}
