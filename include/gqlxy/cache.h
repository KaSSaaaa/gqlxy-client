#pragma once

#include <gqlxy/client/results.h>
#include <optional>

namespace gqlxy {
struct GraphQLResponse;

class Cache {
public:
    virtual ~Cache() = default;
    virtual std::optional<GraphQLResponse> Read(const GraphQLRequest& request) = 0;
    virtual void Write(const GraphQLRequest& request, const GraphQLResponse& result) = 0;
    virtual void Evict(const GraphQLRequest& request) = 0;
};

}
