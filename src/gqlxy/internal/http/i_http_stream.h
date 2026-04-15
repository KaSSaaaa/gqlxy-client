#pragma once

#include <boost/asio/awaitable.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <optional>
#include <rxcpp/rx.hpp>
#include <string>

namespace gqlxy::internal {

struct HttpBodyChunk {
    std::optional<boost::beast::http::header<false>> header;
    std::string data;
    bool done;
};

class IHttpStream {
public:
    virtual ~IHttpStream() = default;

    virtual boost::asio::awaitable<void> Connect(const std::string& host, const std::string& port) = 0;
    virtual boost::asio::awaitable<void> Write(const boost::beast::http::request<boost::beast::http::string_body>& req) = 0;
    virtual rxcpp::observable<HttpBodyChunk> Read() = 0;
    virtual boost::asio::awaitable<void> Shutdown() = 0;
};

}