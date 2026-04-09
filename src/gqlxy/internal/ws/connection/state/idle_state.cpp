#include "idle_state.h"

#include "connecting_state.h"

#include <gqlxy/internal/ws/connection/ws_connection_context.h>

using namespace std;
using namespace gqlxy::internal;
using namespace rxcpp;

IdleState::IdleState(WsConnectionContext& context) : _context(context) {}

void IdleState::Subscribe(const string& id, const GraphQLRequest& req, const subscriber<GraphQLResult>& sub) {
    _context.AddSub(id, req, sub);
    _context.SetState(make_unique<ConnectingState>(_context));
}

void IdleState::Unsubscribe(const string& id) {
    _context.RemoveSub(id);
}
