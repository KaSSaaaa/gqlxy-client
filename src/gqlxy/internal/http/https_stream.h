#pragma once

#include "http_stream_base.h"
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <gqlxy/internal/ssl_context.h>
#include <optional>
#include <string>

namespace gqlxy::internal {

class HttpsStream : public HttpStreamBase<boost::beast::ssl_stream<boost::beast::tcp_stream>> {
public:
    HttpsStream(
        const boost::asio::any_io_executor& ex, const Url& url,
        const std::optional<std::string>& caCert = std::nullopt);

protected:
    boost::asio::awaitable<void> Connect(const std::string& host, const std::string& port) override;
    boost::asio::awaitable<void> Shutdown() override;

private:
    HttpsStream(const boost::asio::any_io_executor& ex, const Url& url, std::unique_ptr<boost::asio::ssl::context> ctx);

    std::unique_ptr<boost::asio::ssl::context> _ctx;
};

}
