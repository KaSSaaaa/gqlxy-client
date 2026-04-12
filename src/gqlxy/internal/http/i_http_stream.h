#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>
#include <string>

namespace gqlxy::internal {

class IHttpStream {
public:
    virtual ~IHttpStream() = default;

    virtual boost::asio::awaitable<void> Connect(const std::string& host, const std::string& port) = 0;
    virtual boost::asio::awaitable<void> Write(const boost::beast::http::request<boost::beast::http::string_body>& req) = 0;
    virtual boost::asio::awaitable<void> ReadHeader(boost::beast::flat_buffer& buf, boost::beast::http::response_parser<boost::beast::http::string_body>& parser) = 0;
    // Returns true if more body data is expected, false when complete or EOF.
    virtual boost::asio::awaitable<bool> ReadBodyChunk(boost::beast::flat_buffer& buf, boost::beast::http::response_parser<boost::beast::http::string_body>& parser) = 0;
    virtual boost::asio::awaitable<void> Shutdown() = 0;
};

}