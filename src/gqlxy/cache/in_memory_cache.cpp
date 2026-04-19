#include <gqlxy/cache/in_memory_cache.h>

#include <gqlxy/parser/ast/document.h>
#include <gqlxy/parser/ast/selection.h>
#include <gqlxy/parser/peg/parser/query/parse_document.h>
#include <gqlxy/utils/optional.h>
#include <gqlxy/utils/ranges.h>
#include <gqlxy/utils/visit.h>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::parser;
using namespace gqlxy::utils;
using namespace nlohmann;

static constexpr string RootQuery = "QUERY_ROOT";
static constexpr string RootMutation = "MUTATION_ROOT";

static string FieldStoreKey(const string& fieldName, const vector<Argument>& args, const json& variables) {
    if (args.empty()) return fieldName;
    auto resolved = json::object();
    for (const auto& arg : args)
        resolved[arg.name] = arg.Value(variables);
    return format("{}({})", fieldName, resolved.dump());
}

InMemoryCache::InMemoryCache(const InMemoryCacheOptions& options) : _options(options) {}

vector<string> InMemoryCache::KeyFieldsFor(const string& typeName) const {
    return and_then(to_optional(_options.typePolicies, _options.typePolicies.find(typeName)), [](const auto& policy) {
        return make_optional(policy.second.keyFields);
    }).value_or(vector<string>{"id"});
}

string InMemoryCache::EntityId(const string& typeName, const json& obj) const {
    auto keyFields = KeyFieldsFor(typeName);
    return make_optional_if(keyFields.size() >= 1 && obj.contains(keyFields[0]), [&]() {
        return format("{}:{}", typeName, to_string(keyFields
            | views::transform([&](const auto& key) { return obj.contains(key) ? obj[key].dump() : ""; })
            | join_with("|")));
    }).value_or("");
}

string InMemoryCache::RootKey(const GraphQLRequest& request) {
    return format("{}:{}", request.type._value == OperationType::MUTATION ? RootMutation : RootQuery, json{
        {"query", request.query},
        {"variables", request.variables}
    }.dump());
}

json InMemoryCache::NormalizeObject(
    const json& obj, const vector<Selection>& selections, const Fragments& fragments,
    const json& variables)
{
    if (!obj.is_object()) return obj;

    auto typeName = obj.value("__typename", "");
    auto normalized = json::object();

    for (const auto& sel : selections) {
        visit(overloaded{
            [&](const Field& field) {
                auto responseKey = field.alias.value_or(field.name);
                if (!obj.contains(responseKey)) return;
                auto value = obj[responseKey];
                auto subSelections = field.selectionSet ? field.selectionSet->selections : vector<Selection>{};
                normalized[FieldStoreKey(responseKey, field.arguments, variables)] = !subSelections.empty()
                    ? NormalizeValue(value, subSelections, fragments, variables)
                    : value;
            },
            [&](const FragmentSpread& spread) {
                if (auto it = fragments.find(spread.name);
                    it != fragments.end() && (typeName.empty() || it->second.typeCondition == typeName))
                    normalized.merge_patch(NormalizeObject(obj, it->second.selectionSet.selections, fragments, variables));
            },
            [&](const InlineFragment& inlineFrag) {
                if (!inlineFrag.typeCondition || typeName.empty() || *inlineFrag.typeCondition == typeName) {
                    auto subSelections = inlineFrag.selectionSet ? inlineFrag.selectionSet->selections : vector<Selection>{};
                    normalized.merge_patch(NormalizeObject(obj, subSelections, fragments, variables));
                }
            }
        }, sel);
    }

    auto id = EntityId(typeName, obj);
    if (!typeName.empty()) normalized["__typename"] = typeName;
    if (id.empty()) return normalized;

    if (_entityStore.contains(id)) _entityStore[id].merge_patch(normalized);
    else _entityStore[id] = normalized;

    return json{{"__ref", id}};
}

json InMemoryCache::NormalizeValue(
    const json& data, const vector<Selection>& selections, const Fragments& fragments,
    const json& variables) {
    switch (data.type()) {
        case json::value_t::null: return nullptr;
        case json::value_t::object: return NormalizeObject(data, selections, fragments, variables);
        case json::value_t::array: {
            auto arr = json::array();
            ranges::transform(data, back_inserter(arr), [&](const auto& item) { return NormalizeValue(item, selections, fragments, variables); });
            return arr;
        }
        default: return data;
    }
}

void InMemoryCache::Write(const GraphQLRequest& request, const GraphQLResponse& result) {
    if (!result.data) return;
    auto doc = ParseDocument(request.query);
    if (doc.operations.empty()) return;
    auto& selections = doc.operations[0].selectionSet.selections;
    unique_lock lock(_mutex);
    _entityStore[RootKey(request)] = NormalizeObject(*result.data, selections, doc.fragments, request.variables);
}

json InMemoryCache::DenormalizeValue(
    const json& storeValue, const vector<Selection>& selections,
    const Fragments& fragments, const json& variables) {
    switch (storeValue.type()) {
        case json::value_t::null: return nullptr;
        case json::value_t::object: {
            if (!storeValue.contains("__ref")) return DenormalizeObject(storeValue, selections, fragments, variables);
            return and_then(to_optional(_entityStore, _entityStore.find(storeValue["__ref"].get<string>())), [&](const auto& it) {
                return make_optional(DenormalizeObject(it.second, selections, fragments, variables));
            }).value_or(nullptr);
        }
        case json::value_t::array: {
            auto arr = json::array();
            ranges::transform(storeValue, back_inserter(arr), [&](const auto& item) { return DenormalizeValue(item, selections, fragments, variables); });
            return arr;
        }
        default: return storeValue;
    }
}

json InMemoryCache::DenormalizeObject(
    const json& entity, const vector<Selection>& selections, const Fragments& fragments,
    const json& variables) {
    if (!entity.is_object()) return entity;

    auto typeName = entity.value("__typename", "");
    auto result = json::object();

    for (const auto& sel : selections) {
        visit(overloaded{
            [&](const Field& field) {
                auto responseKey = field.alias.value_or(field.name);
                auto storeKey = FieldStoreKey(responseKey, field.arguments, variables);
                if (!entity.contains(storeKey)) return;

                auto subSelections = field.selectionSet ? field.selectionSet->selections : vector<Selection>{};
                if (subSelections.empty()) result[responseKey] = entity[storeKey];
                else if (auto val = DenormalizeValue(entity[storeKey], subSelections, fragments, variables);
                         !val.is_null() || entity[storeKey].is_null())
                    result[responseKey] = val;
            },
            [&](const FragmentSpread& spread) {
                if (auto it = fragments.find(spread.name);
                    it != fragments.end() && (typeName.empty() || it->second.typeCondition == typeName))
                    result.merge_patch(DenormalizeObject(entity, it->second.selectionSet.selections, fragments, variables));
            },
            [&](const InlineFragment& inlineFrag) {
                if (!inlineFrag.typeCondition || typeName.empty() || *inlineFrag.typeCondition == typeName) {
                    auto subSelections = inlineFrag.selectionSet ? inlineFrag.selectionSet->selections : vector<Selection>{};
                    result.merge_patch(DenormalizeObject(entity, subSelections, fragments, variables));
                }
            }
        }, sel);
    }

    return result;
}

optional<GraphQLResponse> InMemoryCache::Read(const GraphQLRequest& request) {
    auto doc = ParseDocument(request.query);
    if (doc.operations.empty()) return nullopt;
    auto& selections = doc.operations[0].selectionSet.selections;

    unique_lock lock(_mutex);

    auto rootKey = RootKey(request);
    return and_then(to_optional(_entityStore, _entityStore.find(rootKey)), [&, this](const auto& it) {
        auto data = DenormalizeObject(it.second, selections, doc.fragments, request.variables);
        return make_optional_if(!data.is_null(), [&]() {
            return GraphQLResponse{.data = data};
        });
    });
}

void InMemoryCache::Evict(const GraphQLRequest& request) {
    unique_lock lock(_mutex);
    _entityStore.erase(RootKey(request));
}

void InMemoryCache::EvictEntity(const string& entityId) {
    unique_lock lock(_mutex);
    _entityStore.erase(entityId);
}

json InMemoryCache::Extract() {
    unique_lock lock(_mutex);
    return json(_entityStore);
}
