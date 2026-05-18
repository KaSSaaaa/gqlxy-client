#pragma once

#include <functional>
#include <gqlxy/client/cache.h>
#include <gqlxy/client/client/fetch_policy.h>
#include <gqlxy/client/client/results.h>
#include <gqlxy/client/link.h>
#include <gqlxy/client/observable.h>
#include <gqlxy/core/parser/ast/document.h>
#include <gqlxy/client/transforms/add_typename.h>
#include <memory>
#include <optional>
#include <vector>

namespace gqlxy {

using DocumentTransform = std::function<parser::Document(const parser::Document&)>;

struct ClientOptions {
    std::shared_ptr<Link> link;
    std::shared_ptr<Cache> cache;
    FetchPolicy defaultFetchPolicy = FetchPolicy::CacheFirst;
    std::vector<DocumentTransform> documentTransforms = { AddTypename };
};

struct QueryOptions {
    std::string query;
    nlohmann::json variables = nullptr;
    std::optional<FetchPolicy> fetchPolicy;
};

struct MutationOptions {
    std::string query;
    nlohmann::json variables = nullptr;
};

struct SubscribeOptions {
    std::string query;
    nlohmann::json variables = nullptr;
};

class Client {
public:
    Client(const ClientOptions& options);

    Observable<GraphQLResponse> Query(const QueryOptions& options);
    Observable<GraphQLResponse> Mutation(const MutationOptions& options);
    Observable<GraphQLResponse> Subscribe(const SubscribeOptions& options);
    Observable<GraphQLResponse> Refetch(const QueryOptions& options);

private:
    Observable<GraphQLResponse> Execute(const GraphQLRequest& request);
    Observable<GraphQLResponse> FetchFromNetwork(const GraphQLRequest& request);
    GraphQLRequest BuildRequest(
        const std::string& query, const nlohmann::json& variables, parser::OperationType type, FetchPolicy policy);

    ClientOptions _options;
};

}
