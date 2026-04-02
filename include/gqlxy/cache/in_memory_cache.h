#pragma once

#include <gqlxy/cache.h>
#include <mutex>
#include <string>
#include <unordered_map>

namespace gqlxy {

class InMemoryCache : public Cache {
public:
    std::optional<GraphQLResult> Read(const GraphQLRequest& request) override;
    void Write(const GraphQLRequest& request, const GraphQLResult& result) override;
    void Evict(const GraphQLRequest& request) override;

private:
    std::mutex mutex_;
    std::unordered_map<std::string, GraphQLResult> store_;

    static std::string CacheKey(const GraphQLRequest& request);
};

} // namespace gqlxy
