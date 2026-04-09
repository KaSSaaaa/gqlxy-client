#pragma once

#include <gqlxy/client/results.h>
#include <gqlxy/internal/ws/connection/ws_connection_context.h>
#include <gqlxy/links/ws_link.h>

#include <rxcpp/rx.hpp>

#include <memory>
#include <string>

namespace gqlxy::internal {

class WsConnection {
public:
    explicit WsConnection(const WsLinkOptions& opts);
    ~WsConnection();

    void Subscribe(const std::string& id, const GraphQLRequest& req, const rxcpp::subscriber<GraphQLResult>& sub);
    void Unsubscribe(const std::string& id);
    void Stop();

private:
    std::shared_ptr<WsConnectionContext> _ctx;
};

}
