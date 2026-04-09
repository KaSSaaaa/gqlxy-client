#pragma once

#include "i_connection_state.h"

namespace gqlxy::internal {

class ConnectedState : public IConnectionState {
public:
    ConnectedState(WsConnectionContext& context);

    void Subscribe(const std::string&, const GraphQLRequest&, const rxcpp::subscriber<GraphQLResult>&) override;
    void Unsubscribe(const std::string&) override;
    void OnTransportConnected() override {}
    void OnTransportMessage(const std::string&) override;
    void OnTransportDisconnected() override;

private:
    WsConnectionContext& _context;
};

}
