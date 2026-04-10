#pragma once

#include "i_http_stream.h"

#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>

#include <optional>
#include <string>

namespace gqlxy::internal {

class HttpsStream : public IHttpStream {
public:
    HttpsStream(const boost::asio::any_io_executor& ex, const std::optional<std::string>& caCert = std::nullopt);

    boost::asio::awaitable<void> Connect(const std::string& host, const std::string& port) override;
    boost::asio::awaitable<void> Write(const boost::beast::http::request<boost::beast::http::string_body>& req) override;
    boost::asio::awaitable<void> Read(boost::beast::flat_buffer& buf, boost::beast::http::response<boost::beast::http::string_body>& res) override;
    boost::asio::awaitable<void> Shutdown() override;

private:
    boost::asio::ssl::context _ctx;
    boost::beast::ssl_stream<boost::beast::tcp_stream> _stream;
};

}
