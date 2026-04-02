#include <gqlxy/cache/in_memory_cache.h>
#include <nlohmann/json.hpp>

namespace gqlxy {

std::optional<GraphQLResult> InMemoryCache::Read(const GraphQLRequest& request) {
    std::lock_guard lock(mutex_);
    auto it = store_.find(CacheKey(request));
    if (it == store_.end()) return std::nullopt;
    return it->second;
}

void InMemoryCache::Write(const GraphQLRequest& request, const GraphQLResult& result) {
    std::lock_guard lock(mutex_);
    store_[CacheKey(request)] = result;
}

void InMemoryCache::Evict(const GraphQLRequest& request) {
    std::lock_guard lock(mutex_);
    store_.erase(CacheKey(request));
}

std::string InMemoryCache::CacheKey(const GraphQLRequest& request) {
    nlohmann::json key = {{"query", request.query}, {"variables", request.variables}};
    return key.dump();
}

} // namespace gqlxy
