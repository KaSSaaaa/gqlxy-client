#pragma once

#include <gqlxy/results.h>
#include <optional>

namespace gqlxy {

class Cache {
public:
    virtual ~Cache() = default;
    virtual std::optional<GraphQLResult> Read(const GraphQLRequest& request) = 0;
    virtual void Write(const GraphQLRequest& request, const GraphQLResult& result) = 0;
    virtual void Evict(const GraphQLRequest& request) = 0;
};

} // namespace gqlxy
