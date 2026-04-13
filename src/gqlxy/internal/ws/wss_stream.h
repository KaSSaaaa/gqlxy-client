#pragma once

#include "ws_stream_base.h"

#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl.hpp>
#include <memory>
#include <optional>

namespace gqlxy::internal {

class WssStream : public WsStreamBase<boost::asio::ssl::stream<boost::beast::tcp_stream>> {
public:
    WssStream(
        const Url& url, const std::map<std::string, std::string>& headers,
        const std::optional<std::string>& caCert = std::nullopt);

protected:
    void Handshake(const ConnectCallback& callback) override;

private:
    WssStream(
        const Url& url, const std::map<std::string, std::string>& headers,
        std::unique_ptr<boost::asio::ssl::context> ctx);

    std::unique_ptr<boost::asio::ssl::context> _ctx;
};

}
