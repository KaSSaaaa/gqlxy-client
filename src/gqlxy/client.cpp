#include <gqlxy/client.h>
#include "internal/query_parser.h"

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace nlohmann;

Client::Client(const ClientOptions& options) : _options(options) {}

Observable<GraphQLResult> Client::Query(const QueryOptions& opts) {
    return Execute({
        .query = opts.query,
        .variables = opts.variables,
        .operationName = ParseQuery(opts.query).name,
        .type = OperationType::Query,
        .policy = opts.fetchPolicy.value_or(_options.defaultFetchPolicy)
    });
}

Observable<GraphQLResult> Client::Mutation(const MutationOptions& opts) {
    return Execute({
        .query = opts.query,
        .variables = opts.variables,
        .operationName = ParseQuery(opts.query).name,
        .type = OperationType::Mutation,
        .policy = FetchPolicy::NetworkOnly
    });
}

Observable<GraphQLResult> Client::Subscribe(const SubscribeOptions& opts) {
    return _options.link->Execute({
        .query = opts.query,
        .variables = opts.variables,
        .operationName = ParseQuery(opts.query).name,
        .type = OperationType::Subscription,
        .policy = FetchPolicy::NetworkOnly
    });
}

Observable<GraphQLResult> Client::Refetch(const QueryOptions& opts) {
    return Execute({
        .query = opts.query,
        .variables = opts.variables,
        .operationName = ParseQuery(opts.query).name,
        .type = OperationType::Query,
        .policy = FetchPolicy::NetworkOnly
    });
}

Observable<GraphQLResult> Client::FetchFromNetwork(const GraphQLRequest& request) {
    auto cache = _options.cache;
    auto networkObs = static_cast<rxcpp::observable<GraphQLResult>>(_options.link->Execute(request));

    if (!cache) return networkObs;

    return networkObs.tap([cache, request](const GraphQLResult& result) {
        if (result.data) {
            cache->Write(request, result);
        }
    });
}

Observable<GraphQLResult> Client::Execute(const GraphQLRequest& request) {
    auto cache = _options.cache;
    auto policy = request.policy;

    if (!cache || policy == FetchPolicy::NoCache) return _options.link->Execute(request);

    if (policy == FetchPolicy::NetworkOnly) return FetchFromNetwork(request);

    if (policy == FetchPolicy::CacheFirst) {
        if (auto cached = cache->Read(request)) return rxcpp::observable<>::just(std::move(*cached));
        return FetchFromNetwork(request);
    }

    auto cached = cache->Read(request);
    auto network = static_cast<rxcpp::observable<GraphQLResult>>(FetchFromNetwork(request));

    if (cached) return rxcpp::observable<>::just(std::move(*cached)).concat(network);
    return network;
}
