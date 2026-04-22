#include <boost/asio/post.hpp>
#include <future>
#include <gqlxy/internal/asio_context.h>
#include <gqlxy/internal/http/response.h>
#include <gqlxy/internal/http/serialization.h>
#include <gqlxy/internal/ws/connection/ws_connection_context.h>
#include <gqlxy/internal/ws/ws_transport.h>
#include <gqlxy/results.h>
#include <gqlxy/utils/optional.h>
#include <gqlxy/utils/ranges.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace std::chrono;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace gqlxy::utils;
using namespace boost::asio;
using namespace rxcpp;
using nlohmann::json;

WsConnectionContext::WsConnectionContext(const WsLinkOptions& opts) : _reconnectTimer(AsioContext::Get()), _opts(opts) {
    try {
        _url = ParseWsUrl(_opts.url);
    } catch (...) {
        _initError = current_exception();
    }
}

WsConnectionContext::~WsConnectionContext() {
    Stop();
}

void WsConnectionContext::Subscribe(const string& id, const GraphQLRequest& req, const subscriber<GraphQLResponse>& sub) {
    post(AsioContext::Get(), [self = shared_from_this(), id, req, sub]() {
        if (self->_initError) return sub.on_error(self->_initError);
        if (self->_stopping) return sub.on_completed();
        self->OnSubscribe(id, req, sub);
    });
}

void WsConnectionContext::Unsubscribe(const string& id) {
    post(AsioContext::Get(), [self = shared_from_this(), id]() {
        self->OnUnsubscribe(id);
    });
}

void WsConnectionContext::Stop() {
    if (_stopping.exchange(true)) return;
    if (AsioContext::OnContext()) return StopOnContext();
    promise<void> done;
    post(AsioContext::Get(), [this, &done]() {
        StopOnContext();
        post(AsioContext::Get(), [&done]() { done.set_value(); });
    });
    done.get_future().get();
}

void WsConnectionContext::StopOnContext() {
    _reconnectTimer.cancel();
    for (const auto& [_, subscriber] : _subs | views::values)
        subscriber.on_completed();
    _subs.clear();
    if (_transport) _transport->Close();
}

void WsConnectionContext::OnSubscribe(
    const string& id, const GraphQLRequest& req, const subscriber<GraphQLResponse>& sub) {
    AddSub(id, req, sub);
    switch (_state) {
        case ConnectionState::Idle: TransitionTo(ConnectionState::Connecting); break;
        case ConnectionState::Connected: SendSubscribe(id, req); break;
        default: break;
    }
}

void WsConnectionContext::OnUnsubscribe(const string& id) {
    if (_state == ConnectionState::Connected && !_stopping)
        Send({
            {"type", "complete"},
            {"id", id}
        });
    RemoveSub(id);
    if (_state == ConnectionState::Reconnecting && !HasSubs()) {
        CancelReconnect();
        TransitionTo(ConnectionState::Idle);
    }
}

void WsConnectionContext::OnTransportConnected() {
    if (_state == ConnectionState::Connecting)
        Send({
            {"type", "connection_init"}
        });
}

void WsConnectionContext::OnTransportMessage(const string& raw) {
    if (_state == ConnectionState::Connecting) {
        try {
            if (json::parse(raw).value("type", "") != "connection_ack") return;
        } catch (...) {
            return;
        }
        _everConnected = true;
        _reconnectAttempt = 0;
        ReplaySubs();
        TransitionTo(ConnectionState::Connected);
    } else if (_state == ConnectionState::Connected) {
        Dispatch(raw);
    }
}

void WsConnectionContext::OnTransportDisconnected() {
    _transport.reset();
    if (_state == ConnectionState::Connecting) {
        if (!HasSubs()) return TransitionTo(ConnectionState::Idle);
        if (!_everConnected) {
            FailAll(make_exception_ptr(runtime_error("WsLink: could not connect to " + _opts.url)));
            return TransitionTo(ConnectionState::Idle);
        }
        TransitionTo(ConnectionState::Reconnecting);
    } else if (_state == ConnectionState::Connected) {
        TransitionTo(ConnectionState::Reconnecting);
    }
}

void WsConnectionContext::TransitionTo(ConnectionState state) {
    _state = state;
    switch (_state) {
        case ConnectionState::Connecting: return ConnectTransport();
        case ConnectionState::Reconnecting: return ScheduleReconnect();
        default: break;
    }
}

void WsConnectionContext::AddSub(const string& id, const GraphQLRequest& req, const subscriber<GraphQLResponse>& sub) {
    _subs.emplace(id, WsSubscription {req, sub});
}

void WsConnectionContext::RemoveSub(const string& id) {
    _subs.erase(id);
}

bool WsConnectionContext::HasSubs() const {
    return !_subs.empty();
}

void WsConnectionContext::ReplaySubs() {
    for (const auto& [id, sub] : _subs)
        SendSubscribe(id, sub.request);
}

void WsConnectionContext::FailAll(const exception_ptr& ex) {
    for (const auto& [_, subscriber] : _subs | views::values)
        subscriber.on_error(ex);
    _subs.clear();
}

void WsConnectionContext::CompleteSub(const string& id) {
    if (auto it = _subs.find(id); it != _subs.end()) {
        it->second.subscriber.on_completed();
        _subs.erase(it);
    }
}

void WsConnectionContext::ConnectTransport() {
    _transport = make_shared<WsTransport>(
        _url, _opts.headers,
        WsTransportCallbacks {
            .onConnected = WeakCallback([](auto& ctx) { ctx.OnTransportConnected(); }),
            .onMessage = WeakCallback([](auto& ctx, const string& msg) { ctx.OnTransportMessage(msg); }),
            .onDisconnected = WeakCallback([](auto& ctx) {
                if (!ctx._stopping) ctx.OnTransportDisconnected();
            }),
        },
        _opts.caCert);
    _transport->Connect();
}

void WsConnectionContext::Send(const json& msg) {
    _transport->Send(msg.dump());
}

void WsConnectionContext::SendSubscribe(const string& id, const GraphQLRequest& req) {
    Send({
        {"type", "subscribe"},
        {"id", id},
        {"payload", SerializeRequest(req)}
    });
}

void WsConnectionContext::Dispatch(const string& raw) {
    try {
        auto msg = json::parse(raw);
        auto type = msg.value("type", "");
        auto id = msg.value("id", "");

        if (type == "complete") return CompleteSub(id);
        if (type == "ping") return Send({{"type", "pong"}});

        if (const auto sub = and_then(to_optional(_subs, _subs.find(id)), [&](const auto& it) {
            return make_optional(it.second.subscriber);
        }); sub.has_value()) {
            sub->on_next(type == "next"
                ? ParseJsonPayload(msg["payload"])
                : ParseJsonPayload({
                    {"errors", msg["payload"]}
                }));
            if (type == "error") CompleteSub(id);
        }
    } catch (...) {
    }
}

void WsConnectionContext::ScheduleReconnect() {
    const int delay_s = min(1 << min(_reconnectAttempt, 5), 30);
    ++_reconnectAttempt;
    _reconnectTimer.expires_after(seconds(delay_s));
    _reconnectTimer.async_wait(WeakCallback([](auto& ctx, const auto& ec) {
        if (!ec && !ctx._stopping) ctx.TransitionTo(ConnectionState::Connecting);
    }));
}

void WsConnectionContext::CancelReconnect() {
    _reconnectTimer.cancel();
}
