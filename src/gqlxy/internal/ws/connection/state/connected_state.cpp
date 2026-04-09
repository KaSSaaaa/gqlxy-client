#include "connected_state.h"

#include "reconnecting_state.h"

#include <gqlxy/internal/ws/connection/ws_connection_context.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace gqlxy::internal;
using namespace rxcpp;
using nlohmann::json;

ConnectedState::ConnectedState(WsConnectionContext& context) : _context(context) {}

void ConnectedState::Subscribe(const string& id, const GraphQLRequest& req, const subscriber<GraphQLResult>& sub) {
    _context.AddSub(id, req, sub);
    _context.SendSubscribe(id, req);
}

void ConnectedState::Unsubscribe(const string& id) {
    _context.Send({
        {"type", "complete"},
        {"id", id}
    });
    _context.RemoveSub(id);
}

void ConnectedState::OnTransportMessage(const string& raw) {
    _context.Dispatch(raw);
}

void ConnectedState::OnTransportDisconnected() {
    _context.ResetTransport();
    _context.SetState(make_unique<ReconnectingState>(_context));
}
