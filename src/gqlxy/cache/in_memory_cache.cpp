#include <gqlxy/cache/in_memory_cache.h>
#include <gqlxy/internal/query_parser.h>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace nlohmann;

const string RootQuery = "ROOT_QUERY";
const string RootMutation = "ROOT_MUTATION";

string FieldStoreKey(const string& fieldName, const json& args) {
    if (args.empty()) return fieldName;
    return fieldName + "(" + args.dump() + ")";
}

json ResolveVariables(const json& args, const json& variables) {
    if (args.is_null()) return args;

    auto resolved = json::object();
    for (auto& [key, val] : args.items()) {
        if (val.is_object() && val.contains("$var")) {
            auto varName = val["$var"].get<string>();
            if (!variables.is_null() && variables.contains(varName))
                resolved[key] = variables[varName];
            else
                resolved[key] = nullptr;
        } else {
            resolved[key] = val;
        }
    }
    return resolved;
}

InMemoryCache::InMemoryCache(const InMemoryCacheOptions& options) : _options(options) {}

vector<string> InMemoryCache::KeyFieldsFor(const string& typeName) const {
    auto it = _options.typePolicies.find(typeName);
    if (it != _options.typePolicies.end()) return it->second.keyFields;
    return {"id"};
}

string InMemoryCache::EntityId(const string& typeName, const json& obj) const {
    auto keyFields = KeyFieldsFor(typeName);
    if (keyFields.size() == 1) {
        auto& kf = keyFields[0];
        if (obj.contains(kf)) return typeName + ":" + obj[kf].dump();
        return "";
    }
    string composite = typeName + ":";
    for (size_t i = 0; i < keyFields.size(); ++i) {
        if (!obj.contains(keyFields[i])) return "";
        if (i > 0) composite += "|";
        composite += obj[keyFields[i]].dump();
    }
    return composite;
}

string InMemoryCache::RootKey(const GraphQLRequest& request) {
    auto key = (request.type == OperationType::Mutation) ? RootMutation : RootQuery;
    return key + ":" + json{{"query", request.query}, {"variables", request.variables}}.dump();
}

json NormalizeValue(
    const json& data,
    const vector<Selection>& selections,
    const vector<FragmentDefinition>& fragments,
    const json& variables,
    unordered_map<string, json>& store,
    const function<string(const string&, const json&)>& entityId);

json NormalizeObject(
    const json& obj,
    const vector<Selection>& selections,
    const vector<FragmentDefinition>& fragments,
    const json& variables,
    unordered_map<string, json>& store,
    const function<string(const string&, const json&)>& entityId)
{
    if (!obj.is_object()) return obj;

    auto typeName = obj.value("__typename", "");
    auto id = typeName.empty() ? "" : entityId(typeName, obj);

    json normalized = json::object();

    for (const auto& sel : selections) {
        if (auto* field = get_if<SelectionField>(&sel)) {
            auto responseKey = field->alias.value_or(field->name);
            if (!obj.contains(responseKey)) continue;

            auto resolvedArgs = ResolveVariables(field->arguments, variables);
            auto storeKey = FieldStoreKey(responseKey, resolvedArgs);

            if (field->selections.empty()) {
                normalized[storeKey] = obj[responseKey];
            } else {
                normalized[storeKey] = NormalizeValue(
                    obj[responseKey], field->selections, fragments, variables, store, entityId);
            }
        } else if (auto* spread = get_if<FragmentSpread>(&sel)) {
            for (const auto& frag : fragments) {
                if (frag.name != spread->name) continue;
                if (!typeName.empty() && frag.typeCondition != typeName) continue;
                auto fragNorm = NormalizeObject(obj, frag.selections, fragments, variables, store, entityId);
                normalized.merge_patch(fragNorm);
            }
        } else if (auto* inlineFrag = get_if<InlineFragment>(&sel)) {
            if (inlineFrag->typeCondition && !typeName.empty() && *inlineFrag->typeCondition != typeName)
                continue;
            auto fragNorm = NormalizeObject(obj, inlineFrag->selections, fragments, variables, store, entityId);
            normalized.merge_patch(fragNorm);
        }
    }

    if (!typeName.empty()) normalized["__typename"] = typeName;

    if (!id.empty()) {
        if (store.contains(id)) {
            store[id].merge_patch(normalized);
        } else {
            store[id] = normalized;
        }
        return json{{"__ref", id}};
    }

    return normalized;
}

json NormalizeValue(
    const json& data,
    const vector<Selection>& selections,
    const vector<FragmentDefinition>& fragments,
    const json& variables,
    unordered_map<string, json>& store,
    const function<string(const string&, const json&)>& entityId)
{
    if (data.is_null()) return nullptr;
    if (data.is_array()) {
        auto arr = json::array();
        for (const auto& item : data)
            arr.push_back(NormalizeValue(item, selections, fragments, variables, store, entityId));
        return arr;
    }
    if (data.is_object())
        return NormalizeObject(data, selections, fragments, variables, store, entityId);
    return data;
}

void InMemoryCache::Write(const GraphQLRequest& request, const GraphQLResult& result) {
    if (!result.data) return;

    auto parsed = ParseQuery(request.query);

    auto entityIdFn = [this](const string& typeName, const json& obj) {
        return EntityId(typeName, obj);
    };

    unique_lock lock(_mutex);

    auto rootKey = RootKey(request);
    auto rootObj = json::object();

    for (const auto& sel : parsed.selections) {
        if (auto* field = get_if<SelectionField>(&sel)) {
            auto responseKey = field->alias.value_or(field->name);
            if (!result.data->contains(responseKey)) continue;

            auto resolvedArgs = ResolveVariables(field->arguments, request.variables);
            auto storeKey = FieldStoreKey(responseKey, resolvedArgs);

            if (field->selections.empty()) {
                rootObj[storeKey] = (*result.data)[responseKey];
            } else {
                rootObj[storeKey] = NormalizeValue(
                    (*result.data)[responseKey], field->selections,
                    parsed.fragments, request.variables, _entityStore, entityIdFn);
            }
        } else if (auto* spread = get_if<FragmentSpread>(&sel)) {
            for (const auto& frag : parsed.fragments) {
                if (frag.name != spread->name) continue;
                auto fragNorm = NormalizeObject(
                    *result.data, frag.selections, parsed.fragments,
                    request.variables, _entityStore, entityIdFn);
                rootObj.merge_patch(fragNorm);
            }
        } else if (auto* inlineFrag = get_if<InlineFragment>(&sel)) {
            auto fragNorm = NormalizeObject(
                *result.data, inlineFrag->selections, parsed.fragments,
                request.variables, _entityStore, entityIdFn);
            rootObj.merge_patch(fragNorm);
        }
    }

    _entityStore[rootKey] = rootObj;
}

json DenormalizeValue(
    const json& storeValue,
    const vector<Selection>& selections,
    const vector<FragmentDefinition>& fragments,
    const json& variables,
    const unordered_map<string, json>& store);

json DenormalizeObject(
    const json& entity,
    const vector<Selection>& selections,
    const vector<FragmentDefinition>& fragments,
    const json& variables,
    const unordered_map<string, json>& store)
{
    if (!entity.is_object()) return entity;

    auto typeName = entity.value("__typename", "");
    auto result = json::object();

    for (const auto& sel : selections) {
        if (auto* field = get_if<SelectionField>(&sel)) {
            auto responseKey = field->alias.value_or(field->name);
            auto resolvedArgs = ResolveVariables(field->arguments, variables);
            auto storeKey = FieldStoreKey(responseKey, resolvedArgs);

            if (!entity.contains(storeKey)) return nullptr;

            if (field->selections.empty()) {
                result[responseKey] = entity[storeKey];
            } else {
                auto val = DenormalizeValue(
                    entity[storeKey], field->selections, fragments, variables, store);
                if (val.is_null() && !entity[storeKey].is_null()) return nullptr;
                result[responseKey] = val;
            }
        } else if (auto* spread = get_if<FragmentSpread>(&sel)) {
            for (const auto& frag : fragments) {
                if (frag.name != spread->name) continue;
                if (!typeName.empty() && frag.typeCondition != typeName) continue;
                auto fragResult = DenormalizeObject(entity, frag.selections, fragments, variables, store);
                if (fragResult.is_null()) return nullptr;
                result.merge_patch(fragResult);
            }
        } else if (auto* inlineFrag = get_if<InlineFragment>(&sel)) {
            if (inlineFrag->typeCondition && !typeName.empty() && *inlineFrag->typeCondition != typeName)
                continue;
            auto fragResult = DenormalizeObject(entity, inlineFrag->selections, fragments, variables, store);
            if (fragResult.is_null()) return nullptr;
            result.merge_patch(fragResult);
        }
    }

    return result;
}

json DenormalizeValue(
    const json& storeValue,
    const vector<Selection>& selections,
    const vector<FragmentDefinition>& fragments,
    const json& variables,
    const unordered_map<string, json>& store)
{
    if (storeValue.is_null()) return nullptr;

    if (storeValue.is_object() && storeValue.contains("__ref")) {
        auto ref = storeValue["__ref"].get<string>();
        auto it = store.find(ref);
        if (it == store.end()) return nullptr;
        return DenormalizeObject(it->second, selections, fragments, variables, store);
    }

    if (storeValue.is_array()) {
        auto arr = json::array();
        for (const auto& item : storeValue) {
            arr.push_back(DenormalizeValue(item, selections, fragments, variables, store));
        }
        return arr;
    }

    if (storeValue.is_object())
        return DenormalizeObject(storeValue, selections, fragments, variables, store);

    return storeValue;
}

optional<GraphQLResult> InMemoryCache::Read(const GraphQLRequest& request) {
    auto parsed = ParseQuery(request.query);

    shared_lock lock(_mutex);

    auto rootKey = RootKey(request);
    auto it = _entityStore.find(rootKey);
    if (it == _entityStore.end()) return nullopt;

    auto data = DenormalizeObject(
        it->second, parsed.selections, parsed.fragments, request.variables, _entityStore);

    if (data.is_null()) return nullopt;

    return GraphQLResult{.data = data};
}

void InMemoryCache::Evict(const GraphQLRequest& request) {
    unique_lock lock(_mutex);
    _entityStore.erase(RootKey(request));
}

void InMemoryCache::EvictEntity(const string& entityId) {
    unique_lock lock(_mutex);
    _entityStore.erase(entityId);
}

json InMemoryCache::Extract() const {
    shared_lock lock(_mutex);
    return json(_entityStore);
}
