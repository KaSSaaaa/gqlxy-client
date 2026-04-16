#include "internal/query_parser.h"
#include <gqlxy/client.h>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace nlohmann;

GraphQLRequest BuildRequest(const string& query, const json& variables, OperationType type, FetchPolicy policy) {
    return {
        .query = query,
        .variables = variables,
        .operationName = ParseQuery(query).name,
        .type = type,
        .policy = policy};
}

Client::Client(const ClientOptions& options) : _options(options) {}

Observable<GraphQLResponse> Client::Query(const QueryOptions& opts) {
    return Execute(BuildRequest(
        opts.query, opts.variables, OperationType::Query, opts.fetchPolicy.value_or(_options.defaultFetchPolicy)));
}

Observable<GraphQLResponse> Client::Mutation(const MutationOptions& opts) {
    return Execute(BuildRequest(opts.query, opts.variables, OperationType::Mutation, FetchPolicy::NetworkOnly));
}

Observable<GraphQLResponse> Client::Subscribe(const SubscribeOptions& opts) {
    return _options.link->Execute(
        BuildRequest(opts.query, opts.variables, OperationType::Subscription, FetchPolicy::NetworkOnly));
}

Observable<GraphQLResponse> Client::Refetch(const QueryOptions& opts) {
    return Execute(BuildRequest(opts.query, opts.variables, OperationType::Query, FetchPolicy::NetworkOnly));
}

Observable<GraphQLResponse> Client::FetchFromNetwork(const GraphQLRequest& request) {
    auto cache = _options.cache;
    auto networkObs = static_cast<rxcpp::observable<GraphQLResponse>>(_options.link->Execute(request));

    if (!cache) return networkObs;

    return networkObs.tap([cache, request](const GraphQLResponse& result) {
        if (result.data) cache->Write(request, result);
    });
}

Observable<GraphQLResponse> Client::Execute(const GraphQLRequest& request) {
    auto policy = request.policy;
    if (_options.cache == nullptr) policy = FetchPolicy::NoCache;

    switch (policy) {
        case FetchPolicy::NoCache: return _options.link->Execute(request);
        case FetchPolicy::NetworkOnly: return FetchFromNetwork(request);
        case FetchPolicy::CacheFirst:
            if (auto cached = _options.cache->Read(request)) return rxcpp::observable<>::just(std::move(*cached));
            return FetchFromNetwork(request);
        default:
            auto cached = _options.cache->Read(request);
            auto network = static_cast<rxcpp::observable<GraphQLResponse>>(FetchFromNetwork(request));
            if (cached) return rxcpp::observable<>::just(std::move(*cached)).concat(network);
            return network;
    }
}
