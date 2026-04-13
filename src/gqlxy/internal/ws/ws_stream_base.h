#pragma once
#include "gqlxy/internal/url.h"
#include "i_ws_stream.h"

#include <boost/beast/websocket.hpp>
#include <map>

namespace gqlxy::internal {

template<typename Stream>
class WsStreamBase : public IWsStream {
public:
    void Connect(
        const boost::asio::ip::tcp::resolver::results_type& endpoints, const std::chrono::seconds& timeout,
        const ConnectCallback& callback) override {
        _lowWs.expires_after(timeout);
        _lowWs.async_connect(endpoints, [callback, this](const auto& ec, const auto&) {
            if (ec) return callback(ec);
            Handshake(callback);
        });
    }

    void Read(boost::beast::flat_buffer& buffer, const IOCallback& callback) override {
        _ws.async_read(buffer, callback);
    }

    void Write(const boost::asio::const_buffer& buffer, const IOCallback& callback) override {
        _ws.async_write(buffer, callback);
    }

    void Close() override {
        _lowWs.close();
    }

protected:
    WsStreamBase(
        const Url& url, const std::map<std::string, std::string>& headers,
        boost::beast::websocket::stream<Stream> ws)
        : _url(url),
          _headers(headers),
          _ws(std::move(ws)),
          _lowWs(boost::beast::get_lowest_layer(_ws)) {}

    virtual void Handshake(const ConnectCallback& callback) {
        _lowWs.expires_never();
        _ws.set_option(boost::beast::websocket::stream_base::decorator([this](auto& req) {
            for (const auto& [k, v] : _headers)
                req.set(k, v);
        }));
        _ws.async_handshake(_url.host, _url.target, [callback](const auto& ec) {
            callback(ec);
        });
    }

    Url _url;
    std::map<std::string, std::string> _headers;
    boost::beast::websocket::stream<Stream> _ws;
    boost::beast::lowest_layer_type<boost::beast::websocket::stream<Stream>>& _lowWs;
};

}