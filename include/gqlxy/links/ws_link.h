#pragma once

#include <gqlxy/link.h>
#include <string>

namespace gqlxy {

struct WsLinkOptions {
    std::string url;
};

// WebSocket link implementing the graphql-transport-ws protocol.
class WsLink : public Link {
public:
    explicit WsLink(WsLinkOptions options);

    Observable<GraphQLResult> Execute(const GraphQLRequest& request) override;

private:
    WsLinkOptions _options;
};

}
