#pragma once

#include <boost/beast/core.hpp>
#include <deque>
#include <functional>
#include <gqlxy/client/internal/url.h>
#include <gqlxy/client/internal/ws/i_ws_stream.h>
#include <gqlxy/client/link.h>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace gqlxy::internal {

struct WsTransportCallbacks {
    std::function<void()> onConnected;
    std::function<void(std::string)> onMessage;
    std::function<void()> onDisconnected;
};

class WsTransport : public std::enable_shared_from_this<WsTransport> {
public:
    WsTransport(
        const Url& url, const Headers& headers, const WsTransportCallbacks& cbs,
        const std::optional<std::string>& caCert = std::nullopt);

    void Connect();
    void Send(const std::string& msg);
    void Close();

private:
    Url _url;
    Headers _headers;
    WsTransportCallbacks _cbs;
    std::optional<std::string> _caCert;

    std::shared_ptr<IWsStream> _stream;
    boost::beast::flat_buffer _readBuf;
    std::deque<std::string> _writeQueue;
    bool _writing {false};
    bool _disconnected {false};

    void CreateStream();
    void OnResolved(const boost::beast::error_code& ec, const boost::asio::ip::tcp::resolver::results_type& endpoints);
    void Read();
    void OnRead(const boost::beast::error_code& ec, std::size_t bytes);
    void Write();
    void OnWrite(const boost::beast::error_code& ec);
    void FireDisconnected();
};

}
