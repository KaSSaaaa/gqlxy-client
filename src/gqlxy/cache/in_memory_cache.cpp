#include <gqlxy/cache/in_memory_cache.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace gqlxy;
using namespace nlohmann;

optional<GraphQLResult> InMemoryCache::Read(const GraphQLRequest& request) {
    lock_guard lock(_mutex);
    auto it = _store.find(CacheKey(request));
    if (it == _store.end()) return nullopt;
    return it->second;
}

void InMemoryCache::Write(const GraphQLRequest& request, const GraphQLResult& result) {
    lock_guard lock(_mutex);
    _store[CacheKey(request)] = result;
}

void InMemoryCache::Evict(const GraphQLRequest& request) {
    lock_guard lock(_mutex);
    _store.erase(CacheKey(request));
}

string InMemoryCache::CacheKey(const GraphQLRequest& request) {
    return json {
        {"query", request.query},
        {"variables", request.variables}
    }.dump();
}
