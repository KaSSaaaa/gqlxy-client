#pragma once

#include <gqlxy/cache.h>
#include <gqlxy/cache/type_policy.h>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace gqlxy {

class InMemoryCache : public Cache {
public:
    InMemoryCache() = default;
    explicit InMemoryCache(const InMemoryCacheOptions& options);

    std::optional<GraphQLResult> Read(const GraphQLRequest& request) override;
    void Write(const GraphQLRequest& request, const GraphQLResult& result) override;
    void Evict(const GraphQLRequest& request) override;

    void EvictEntity(const std::string& entityId);
    nlohmann::json Extract() const;

private:
    mutable std::shared_mutex _mutex;
    std::unordered_map<std::string, nlohmann::json> _entityStore;
    InMemoryCacheOptions _options;

    std::string EntityId(const std::string& typeName, const nlohmann::json& obj) const;
    std::vector<std::string> KeyFieldsFor(const std::string& typeName) const;

    static std::string RootKey(const GraphQLRequest& request);
};

}
