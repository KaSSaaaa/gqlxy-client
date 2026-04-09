#include "reconnecting_state.h"

#include "idle_state.h"

#include <gqlxy/internal/ws/connection/ws_connection_context.h>

using namespace std;
using namespace gqlxy::internal;
using namespace rxcpp;

ReconnectingState::ReconnectingState(WsConnectionContext& context) : _context(context) {}

void ReconnectingState::OnEnter() {
    _context.ScheduleReconnect();
}

void ReconnectingState::Subscribe(const string& id, const GraphQLRequest& req, const subscriber<GraphQLResult>& sub) {
    _context.AddSub(id, req, sub);
}

void ReconnectingState::Unsubscribe(const string& id) {
    _context.RemoveSub(id);
    if (!_context.HasSubs()) {
        _context.CancelReconnect();
        _context.SetState(make_unique<IdleState>(_context));
    }
}
