#include <gqlxy/client/internal/ws/ws_transport.h>

#include "ws_stream.h"
#include "wss_stream.h"

#include <gqlxy/client/internal/asio_context.h>
#include <iostream>

using namespace std;
using namespace std::chrono;
using namespace gqlxy::internal;
namespace net = boost::asio;
namespace beast = boost::beast;
using namespace boost::beast::websocket;
using tcp = net::ip::tcp;

WsTransport::WsTransport(
    const Url& url, const Headers& headers, const WsTransportCallbacks& cbs, const optional<string>& caCert)
    : _url(url),
      _headers(headers),
      _cbs(cbs),
      _caCert(caCert) {}

void WsTransport::Connect() {
    CreateStream();
    auto resolver = make_shared<tcp::resolver>(AsioContext::Get());
    resolver->async_resolve(
        _url.host, _url.port, [self = shared_from_this(), resolver](const auto& ec, const auto& endpoints) {
            self->OnResolved(ec, endpoints);
        });
}

void WsTransport::Send(const string& msg) {
    _writeQueue.push_back(msg);
    if (!_writing) {
        _writing = true;
        Write();
    }
}

void WsTransport::Close() {
    if (_stream) _stream->Close();
}

void WsTransport::FireDisconnected() {
    if (_disconnected) return;
    _disconnected = true;
    _cbs.onDisconnected();
}

void WsTransport::CreateStream() {
    _headers["Sec-WebSocket-Protocol"] = "graphql-transport-ws";
    _stream = _url.tls ? static_pointer_cast<IWsStream>(make_shared<WssStream>(_url, _headers, _caCert))
                       : make_shared<WsStream>(_url, _headers);
}

void WsTransport::OnResolved(const beast::error_code& ec, const tcp::resolver::results_type& endpoints) {
    if (ec) return FireDisconnected();
    _stream->Connect(endpoints, seconds(30), [self = shared_from_this()](const auto& error) {
        if (error) return self->FireDisconnected();
        self->_cbs.onConnected();
        self->Read();
    });
}

void WsTransport::Read() {
    _readBuf.clear();
    _stream->Read(_readBuf, [self = shared_from_this()](const auto& ec, const auto bytes) {
        self->OnRead(ec, bytes);
    });
}

void WsTransport::OnRead(const beast::error_code& ec, size_t) {
    if (ec) return FireDisconnected();
    _cbs.onMessage(beast::buffers_to_string(_readBuf.data()));
    Read();
}

void WsTransport::Write() {
    _stream->Write(net::buffer(_writeQueue.front()), [self = shared_from_this()](const auto& ec, size_t) {
        self->OnWrite(ec);
    });
}

void WsTransport::OnWrite(const beast::error_code& ec) {
    _writeQueue.pop_front();
    if (ec) {
        _writeQueue.clear();
        _writing = false;
        return FireDisconnected();
    }
    if (_writeQueue.empty()) {
        _writing = false;
        return;
    }
    Write();
}
