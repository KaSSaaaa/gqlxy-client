#pragma once

#include <gqlxy/cache.h>
#include <gqlxy/client/fetch_policy.h>
#include <gqlxy/link.h>
#include <gqlxy/observable.h>
#include <gqlxy/client/results.h>
#include <memory>
#include <optional>

namespace gqlxy {

struct ClientOptions {
    std::shared_ptr<Link> link;
    std::shared_ptr<Cache> cache;
    FetchPolicy defaultFetchPolicy = FetchPolicy::CacheFirst;
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

    ClientOptions _options;
};

}
