#include <gqlxy/internal/ws/connection/ws_connection_context.h>

#include <gqlxy/internal/asio_context.h>
#include <gqlxy/internal/http/response.h>
#include <gqlxy/internal/http/serialization.h>
#include <gqlxy/internal/ws/connection/state/connecting_state.h>
#include <gqlxy/internal/ws/connection/state/i_connection_state.h>
#include <gqlxy/internal/ws/connection/state/idle_state.h>
#include <gqlxy/internal/ws/ws_transport.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace boost::asio;
using namespace rxcpp;
using nlohmann::json;

static json MakeSubscribe(const string& id, const GraphQLRequest& req) {
    return {
        {"type", "subscribe"},
        {"id", id},
        {"payload", SerializeRequest(req)}
    };
}

WsConnectionContext::WsConnectionContext(const WsLinkOptions& opts)
    : _state(make_unique<IdleState>(*this)),
      _reconnectTimer(AsioContext::Get()),
      _opts(opts) {
    try {
        _url = ParseWsUrl(_opts.url);
    } catch (...) {
        _initError = current_exception();
    }
}

WsConnectionContext::~WsConnectionContext() = default;

void WsConnectionContext::AddSub(const string& id, const GraphQLRequest& req, const subscriber<GraphQLResult>& sub) {
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
    for (auto& [_, subscriber] : _subs | views::values)
        subscriber.on_error(ex);
    _subs.clear();
}

void WsConnectionContext::Connect() {
    _transport = make_shared<WsTransport>(
        _url, _opts.headers,
        WsTransportCallbacks {
            .onConnected =
                [weak = weak_from_this()]() {
                    if (auto ctx = weak.lock()) ctx->_state->OnTransportConnected();
                },
            .onMessage =
                [weak = weak_from_this()](const string& msg) {
                    if (auto ctx = weak.lock()) ctx->_state->OnTransportMessage(msg);
                },
            .onDisconnected =
                [weak = weak_from_this()]() {
                    if (auto ctx = weak.lock(); ctx && !ctx->_stopping) ctx->_state->OnTransportDisconnected();
                },
        });
    _transport->Connect();
}

void WsConnectionContext::Send(const json& msg) {
    _transport->Send(msg.dump());
}

void WsConnectionContext::SendSubscribe(const string& id, const GraphQLRequest& req) {
    Send(MakeSubscribe(id, req));
}

void WsConnectionContext::ResetTransport() {
    _transport.reset();
}

void WsConnectionContext::Dispatch(const string& raw) {
    try {
        const auto msg = json::parse(raw);
        const auto type = msg.value("type", "");
        const auto id = msg.value("id", "");
        if (type == "next") {
            if (auto it = _subs.find(id); it != _subs.end())
                it->second.subscriber.on_next(ParseJsonPayload(msg["payload"]));
        } else if (type == "complete") {
            CompleteSub(id);
        } else if (type == "error") {
            if (auto it = _subs.find(id); it != _subs.end()) {
                it->second.subscriber.on_next(ParseJsonPayload(json {
                    {"errors", msg["payload"]}
                }));
                CompleteSub(id);
            }
        } else if (type == "ping") {
            Send({
                {"type", "pong"}
            });
        }
    } catch (...) {
    }
}

void WsConnectionContext::SetConnected() {
    _everConnected = true;
    _reconnectAttempt = 0;
}

bool WsConnectionContext::IsEverConnected() const {
    return _everConnected;
}

string WsConnectionContext::GetUrl() const {
    return _opts.url;
}

void WsConnectionContext::ScheduleReconnect() {
    const int delay_s = min(1 << min(_reconnectAttempt, 5), 30);
    ++_reconnectAttempt;
    _reconnectTimer.expires_after(std::chrono::seconds(delay_s));
    _reconnectTimer.async_wait([weak = weak_from_this()](const auto& ec) {
        if (auto ctx = weak.lock(); ctx && !ec && !ctx->_stopping) ctx->SetState(make_unique<ConnectingState>(*ctx));
    });
}

void WsConnectionContext::CancelReconnect() {
    _reconnectTimer.cancel();
}

void WsConnectionContext::SetState(unique_ptr<IConnectionState> next) {
    auto old = std::move(_state);
    _state = std::move(next);
    _state->OnEnter();
}

void WsConnectionContext::CompleteSub(const string& id) {
    if (auto it = _subs.find(id); it != _subs.end()) {
        it->second.subscriber.on_completed();
        _subs.erase(it);
    }
}

void WsConnectionContext::DispatchSubscribe(
    const string& id, const GraphQLRequest& req, const subscriber<GraphQLResult>& sub) {
    _state->Subscribe(id, req, sub);
}

void WsConnectionContext::DispatchUnsubscribe(const string& id) {
    _state->Unsubscribe(id);
}

void WsConnectionContext::Stop() {
    if (_stopping.exchange(true)) return;

    _reconnectTimer.cancel();
    for (const auto& sub : _subs | views::values)
        sub.subscriber.on_completed();
    _subs.clear();
    if (_transport) _transport->Close();
}

bool WsConnectionContext::IsStopping() const {
    return _stopping;
}

exception_ptr WsConnectionContext::InitError() const {
    return _initError;
}
