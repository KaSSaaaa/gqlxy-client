#include "connecting_state.h"

#include "connected_state.h"
#include "idle_state.h"
#include "reconnecting_state.h"

#include <gqlxy/internal/ws/connection/ws_connection_context.h>
#include <nlohmann/json.hpp>
#include <stdexcept>

using namespace std;
using namespace gqlxy::internal;
using namespace rxcpp;
using nlohmann::json;

ConnectingState::ConnectingState(WsConnectionContext& context) : _context(context) {}

void ConnectingState::OnEnter() {
    _context.Connect();
}

void ConnectingState::Subscribe(const string& id, const GraphQLRequest& req, const subscriber<GraphQLResult>& sub) {
    _context.AddSub(id, req, sub);
}

void ConnectingState::Unsubscribe(const string& id) {
    _context.RemoveSub(id);
}

void ConnectingState::OnTransportConnected() {
    _context.Send({{"type", "connection_init"}});
}

void ConnectingState::OnTransportMessage(const string& raw) {
    try {
        if (json::parse(raw).value("type", "") != "connection_ack") return;
    } catch (...) {
        return;
    }
    _context.SetConnected();
    _context.ReplaySubs();
    _context.SetState(make_unique<ConnectedState>(_context));
}

void ConnectingState::OnTransportDisconnected() {
    _context.ResetTransport();
    if (!_context.HasSubs()) return _context.SetState(make_unique<IdleState>(_context));
    if (!_context.IsEverConnected()) {
        _context.FailAll(make_exception_ptr(runtime_error("WsLink: could not connect to " + _context.GetUrl())));
        return _context.SetState(make_unique<IdleState>(_context));
    }
    _context.SetState(make_unique<ReconnectingState>(_context));
}
