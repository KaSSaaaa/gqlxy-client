#pragma once

#include "ws_stream_base.h"

#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl.hpp>
#include <memory>

namespace gqlxy::internal {

class WssStream : public WsStreamBase<boost::asio::ssl::stream<boost::beast::tcp_stream>> {
public:
    WssStream(
        const ParsedUrl& url, const std::map<std::string, std::string>& headers, const WsTransportCallbacks& callbacks);

protected:
    void Handshake(const ConnectCallback& callback) override;

private:
    WssStream(
        const ParsedUrl& url, const std::map<std::string, std::string>& headers, const WsTransportCallbacks& callbacks,
        std::unique_ptr<boost::asio::ssl::context> ctx);

    static std::unique_ptr<boost::asio::ssl::context> MakeSslCtx();

    std::unique_ptr<boost::asio::ssl::context> _ctx;
};

}
