#pragma once

#include <gqlxy/link.h>
#include <string>

namespace gqlxy {

struct SseLinkOptions {
    std::string url;
};

// SSE link implementing the graphql-sse distinct-connections protocol.
class SseLink : public Link {
public:
    explicit SseLink(SseLinkOptions options);

    Observable<GraphQLResult> Execute(const GraphQLRequest& request) override;

private:
    SseLinkOptions options_;
};

} // namespace gqlxy
