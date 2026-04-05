#include <gqlxy/internal/ws/ws_connection.h>

#include <gqlxy/internal/http/response.h>
#include <gqlxy/internal/http/serialization.h>

#include <boost/uuid/random_generator.hpp>
#include <openssl/ssl.h>

#include <algorithm>
#include <stdexcept>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace rxcpp;
namespace beast = boost::beast;
using namespace boost::beast::websocket;
using nlohmann::json;

//TODO Refactor

static string MakeSubscribe(const string& id, const GraphQLRequest& req) {
    return json {{"type", "subscribe"}, {"id", id}, {"payload", SerializeRequest(req)}}.dump();
}

WsConnection::WsConnection(const WsLinkOptions& opts)
    : _opts(opts),
      _work(make_work_guard(_ioc)),
      _thread([this] { _ioc.run(); }),
      _reconnectTimer(_ioc) {
    try {
        _url = ParseWsUrl(_opts.url);
    } catch (...) {
        _initError = current_exception();
    }
}

WsConnection::~WsConnection() {
    _stopping = true;
    post(_ioc, [this]() {
        _reconnectTimer.cancel();
        for (auto& sub : _subs | views::values)
            sub.subscriber.on_completed();
        _subs.clear();
        if (_plainWs || _tlsWs) WithWs([](auto& ws) {
            beast::get_lowest_layer(ws).close();
        });
    });
    _work.reset();
    if (_thread.joinable()) _thread.join();
}

void WsConnection::Subscribe(const string& id, const GraphQLRequest& req, const subscriber<GraphQLResult>& sub) {
    post(_ioc, [self = shared_from_this(), id, req, sub]() mutable {
        if (self->_initError) {
            sub.on_error(self->_initError);
            return;
        }
        if (self->_stopping) {
            sub.on_completed();
            return;
        }
        self->_subs.emplace(id, WsSubscription {req, sub});
        if (self->_state == State::Connected) self->SendSubscribe(id);
        else if (self->_state == State::Idle) self->DoConnect();
    });
}

void WsConnection::Unsubscribe(const string& id) {
    post(_ioc, [self = shared_from_this(), id]() {
        if (auto it = self->_subs.find(id); it != self->_subs.end()) {
            if (self->_state == State::Connected) self->EnqueueWrite(json {{"type", "complete"}, {"id", id}}.dump());
            self->_subs.erase(it);
        }
    });
}

// ─── Connection setup ─────────────────────────────────────────────────────────

void WsConnection::CreateStream() {
    _plainWs.reset();
    _tlsWs.reset();
    if (_url.tls) {
        _sslCtx = make_unique<ssl::context>(ssl::context::tlsv13_client);
        _sslCtx->set_default_verify_paths();
        _sslCtx->set_verify_mode(ssl::verify_peer);
        _tlsWs = make_unique<TlsWs>(_ioc, *_sslCtx);
    } else {
        _plainWs = make_unique<PlainWs>(_ioc);
    }
}

void WsConnection::DoConnect() {
    _state = State::Connecting;
    CreateStream();
    beast::error_code ec;
    tcp::resolver resolver {_ioc};
    auto endpoints = resolver.resolve(_url.host, _url.port, ec);
    if (ec) {
        ScheduleReconnect();
        return;
    }
    WithWs([&](auto& ws) {
        beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));
        beast::get_lowest_layer(ws).async_connect(
            endpoints, [self = shared_from_this()](beast::error_code ec, auto) { self->OnTcpConnected(ec); });
    });
}

void WsConnection::OnTcpConnected(beast::error_code ec) {
    if (ec) {
        ScheduleReconnect();
        return;
    }
    WithWs([](auto& ws) { beast::get_lowest_layer(ws).expires_never(); });
    if (_url.tls) {
        SSL_set_tlsext_host_name(_tlsWs->next_layer().native_handle(), _url.host.c_str());
        _tlsWs->next_layer().async_handshake(
            ssl::stream_base::client, [self = shared_from_this()](beast::error_code ec) { self->OnSslHandshake(ec); });
    } else {
        DoWsHandshake();
    }
}

void WsConnection::OnSslHandshake(beast::error_code ec) {
    if (ec) {
        ScheduleReconnect();
        return;
    }
    DoWsHandshake();
}

void WsConnection::DoWsHandshake() {
    WithWs([this](auto& ws) {
        ws.set_option(stream_base::decorator([this](request_type& req) {
            req.set("Sec-WebSocket-Protocol", "graphql-transport-ws");
            for (const auto& [k, v] : _opts.headers)
                req.set(k, v);
        }));
        ws.async_handshake(
            _url.host, _url.target, [self = shared_from_this()](beast::error_code ec) { self->OnWsHandshake(ec); });
    });
}

void WsConnection::OnWsHandshake(beast::error_code ec) {
    if (ec) {
        ScheduleReconnect();
        return;
    }
    EnqueueWrite(json {{"type", "connection_init"}}.dump());
    DoRead();
}

// ─── Read / Dispatch ──────────────────────────────────────────────────────────

void WsConnection::DoRead() {
    _readBuf.clear();
    WithWs([this](auto& ws) {
        ws.async_read(
            _readBuf, [self = shared_from_this()](beast::error_code ec, size_t bytes) { self->OnRead(ec, bytes); });
    });
}

void WsConnection::OnRead(beast::error_code ec, size_t) {
    if (ec) {
        ScheduleReconnect();
        return;
    }
    try {
        const auto msg = json::parse(beast::buffers_to_string(_readBuf.data()));
        Dispatch(msg);
    } catch (...) {
    }
    DoRead();
}

void WsConnection::Dispatch(const json& msg) {
    const auto type = msg.value("type", "");
    const auto id = msg.value("id", "");
    if (type == "connection_ack") {
        _state = State::Connected;
        _reconnectAttempt = 0;
        _everConnected = true;
        ReplaySubscriptions();
    } else if (type == "next") {
        if (auto it = _subs.find(id); it != _subs.end())
            it->second.subscriber.on_next(ParseJsonPayload(msg["payload"]));
    } else if (type == "complete") {
        CompleteSub(id);
    } else if (type == "error") {
        if (auto it = _subs.find(id); it != _subs.end()) {
            it->second.subscriber.on_next(ParseJsonPayload(json {{"errors", msg["payload"]}}));
            CompleteSub(id);
        }
    } else if (type == "ping") {
        EnqueueWrite(json {{"type", "pong"}}.dump());
    }
}

// ─── Write ────────────────────────────────────────────────────────────────────

void WsConnection::SendSubscribe(const string& id) {
    EnqueueWrite(MakeSubscribe(id, _subs.at(id).request));
}

void WsConnection::EnqueueWrite(const string& msg) {
    _writeQueue.push_back(msg);
    if (!_writing) {
        _writing = true;
        DoWrite();
    }
}

void WsConnection::DoWrite() {
    WithWs([this](auto& ws) {
        ws.async_write(buffer(_writeQueue.front()), [self = shared_from_this()](beast::error_code ec, size_t) {
            self->OnWrite(ec);
        });
    });
}

void WsConnection::OnWrite(beast::error_code ec) {
    _writeQueue.pop_front();
    if (ec) {
        _writeQueue.clear();
        _writing = false;
        ScheduleReconnect();
        return;
    }
    if (_writeQueue.empty()) {
        _writing = false;
        return;
    }
    DoWrite();
}

// ─── Subscriber helpers ───────────────────────────────────────────────────────

void WsConnection::CompleteSub(const string& id) {
    if (auto it = _subs.find(id); it != _subs.end()) {
        it->second.subscriber.on_completed();
        _subs.erase(it);
    }
}

void WsConnection::FailAll(exception_ptr ex) {
    for (auto& sub : _subs | views::values)
        sub.subscriber.on_error(ex);
    _subs.clear();
}

// ─── Reconnect ────────────────────────────────────────────────────────────────

void WsConnection::ScheduleReconnect() {
    if (_stopping || _state == State::Reconnecting) return;
    if (_plainWs || _tlsWs) WithWs([](auto& ws) { beast::get_lowest_layer(ws).close(); });
    if (_subs.empty()) {
        _state = State::Idle;
        return;
    }
    if (!_everConnected) {
        _state = State::Idle;
        FailAll(make_exception_ptr(runtime_error("WsLink: could not connect to " + _opts.url)));
        return;
    }
    _state = State::Reconnecting;
    _writeQueue.clear();
    _writing = false;
    const int delay_s = min(1 << min(_reconnectAttempt, 5), 30);
    ++_reconnectAttempt;
    _reconnectTimer.expires_after(std::chrono::seconds(delay_s));
    _reconnectTimer.async_wait([self = shared_from_this()](beast::error_code ec) {
        if (!ec && !self->_subs.empty() && !self->_stopping) self->DoConnect();
    });
}

void WsConnection::ReplaySubscriptions() {
    for (const auto& id : _subs | views::keys)
        SendSubscribe(id);
}
