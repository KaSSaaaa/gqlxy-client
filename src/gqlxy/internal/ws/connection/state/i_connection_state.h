#pragma once

#include <gqlxy/client/results.h>
#include <rxcpp/rx.hpp>
#include <string>

namespace gqlxy::internal {

class WsConnectionContext;

class IConnectionState {
public:
    virtual ~IConnectionState() = default;
    virtual void OnEnter() {}
    virtual void Subscribe(const std::string&, const GraphQLRequest&, const rxcpp::subscriber<GraphQLResult>&) = 0;
    virtual void Unsubscribe(const std::string&) = 0;
    virtual void OnTransportConnected() = 0;
    virtual void OnTransportMessage(const std::string&) = 0;
    virtual void OnTransportDisconnected() = 0;
};

}
