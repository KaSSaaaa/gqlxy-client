#pragma once

#include <gqlxy/cache.h>
#include <gqlxy/cache/type_policy.h>
#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace gqlxy {
namespace internal {
struct ParsedSelection;
struct FragmentDefinition;
}

class InMemoryCache : public Cache {
public:
    InMemoryCache() = default;
    explicit InMemoryCache(const InMemoryCacheOptions& options);

    std::optional<GraphQLResponse> Read(const GraphQLRequest& request) override;
    void Write(const GraphQLRequest& request, const GraphQLResponse& result) override;
    void Evict(const GraphQLRequest& request) override;

    void EvictEntity(const std::string& entityId);
    nlohmann::json Extract();

private:
    std::mutex _mutex;
    std::unordered_map<std::string, nlohmann::json> _entityStore;
    InMemoryCacheOptions _options;

    std::string EntityId(const std::string& typeName, const nlohmann::json& obj) const;
    std::vector<std::string> KeyFieldsFor(const std::string& typeName) const;

    static std::string RootKey(const GraphQLRequest& request);

    nlohmann::json NormalizeObject(
        const nlohmann::json& obj, const std::vector<internal::ParsedSelection>& selections,
        const std::vector<internal::FragmentDefinition>& fragments, const nlohmann::json& variables);
    nlohmann::json NormalizeValue(
        const nlohmann::json& data, const std::vector<internal::ParsedSelection>& selections,
        const std::vector<internal::FragmentDefinition>& fragments, const nlohmann::json& variables);

    nlohmann::json DenormalizeValue(
        const nlohmann::json& storeValue, const std::vector<internal::ParsedSelection>& selections,
        const std::vector<internal::FragmentDefinition>& fragments, const nlohmann::json& variables);

    nlohmann::json DenormalizeObject(
        const nlohmann::json& entity, const std::vector<internal::ParsedSelection>& selections,
        const std::vector<internal::FragmentDefinition>& fragments, const nlohmann::json& variables);
};

}
