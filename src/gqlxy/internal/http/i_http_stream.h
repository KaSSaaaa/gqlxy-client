#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/message_fwd.hpp>
#include <boost/beast/http/string_body_fwd.hpp>
#include <string>

namespace gqlxy::internal {

class IHttpStream {
public:
    virtual ~IHttpStream() = default;

    virtual boost::asio::awaitable<void> Connect(const std::string& host, const std::string& port) = 0;
    virtual boost::asio::awaitable<void> Write(const boost::beast::http::request<boost::beast::http::string_body>& req) = 0;
    virtual boost::asio::awaitable<void> Read(boost::beast::flat_buffer& buf, boost::beast::http::response<boost::beast::http::string_body>& res) = 0;
    virtual boost::asio::awaitable<void> Shutdown() = 0;
};

}