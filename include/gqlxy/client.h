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

    Observable<GraphQLResult> Query(const QueryOptions& options);
    Observable<GraphQLResult> Mutation(const MutationOptions& options);
    Observable<GraphQLResult> Subscribe(const SubscribeOptions& options);
    Observable<GraphQLResult> Refetch(const QueryOptions& options);

private:
    Observable<GraphQLResult> Execute(const GraphQLRequest& request);
    Observable<GraphQLResult> FetchFromNetwork(const GraphQLRequest& request);

    ClientOptions _options;
};

}
