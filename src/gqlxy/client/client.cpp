#include <gqlxy/client/client.h>
#include <gqlxy/core/parser/peg/parser/query/parse_document.h>
#include <gqlxy/client/print.h>
#include <gqlxy/core/results.h>
#include <gqlxy/core/utils/ranges.h>
#include <rpp/operators/tap.hpp>
#include <rpp/sources/concat.hpp>
#include <rpp/sources/just.hpp>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::parser;
using namespace gqlxy::utils;
using namespace nlohmann;
using namespace rpp;
using namespace rpp::source;
using namespace rpp::operators;

Client::Client(const ClientOptions& options) : _options(options) {}

GraphQLRequest Client::BuildRequest(
    const string& query,
    const json& variables,
    OperationType type,
    FetchPolicy policy) {
    auto doc = ParseDocument(query);

    for (const auto& transform : _options.documentTransforms)
        doc = transform(doc);

    return {
        .query = Print(doc),
        .variables = variables,
        .operationName = doc.operations.empty() ? nullopt : doc.operations[0].name,
        .type = type,
        .policy = policy
    };
}

Observable<GraphQLResponse> Client::Query(const QueryOptions& opts) {
    return Execute(BuildRequest(
        opts.query, opts.variables, OperationType::QUERY, opts.fetchPolicy.value_or(_options.defaultFetchPolicy)));
}

Observable<GraphQLResponse> Client::Mutation(const MutationOptions& opts) {
    return Execute(BuildRequest(opts.query, opts.variables, OperationType::MUTATION, FetchPolicy::NetworkOnly));
}

Observable<GraphQLResponse> Client::Subscribe(const SubscribeOptions& opts) {
    return _options.link->Execute(
        BuildRequest(opts.query, opts.variables, OperationType::SUBSCRIPTION, FetchPolicy::NetworkOnly));
}

Observable<GraphQLResponse> Client::Refetch(const QueryOptions& opts) {
    return Execute(BuildRequest(opts.query, opts.variables, OperationType::QUERY, FetchPolicy::NetworkOnly));
}

Observable<GraphQLResponse> Client::FetchFromNetwork(const GraphQLRequest& request) {
    auto cache = _options.cache;
    dynamic_observable<GraphQLResponse> networkObs = _options.link->Execute(request);

    if (!cache) return networkObs;

    return (networkObs | tap([cache, request](const GraphQLResponse& result) {
        if (result.data) cache->Write(request, result);
    })).as_dynamic();
}

Observable<GraphQLResponse> Client::Execute(const GraphQLRequest& request) {
    auto policy = request.policy;
    if (_options.cache == nullptr) policy = FetchPolicy::NoCache;

    switch (policy) {
        case FetchPolicy::NoCache: return _options.link->Execute(request);
        case FetchPolicy::NetworkOnly: return FetchFromNetwork(request);
        case FetchPolicy::CacheFirst:
            if (auto cached = _options.cache->Read(request)) return just(std::move(*cached)).as_dynamic();
            return FetchFromNetwork(request);
        default:
            auto cached = _options.cache->Read(request);
            dynamic_observable<GraphQLResponse> network = FetchFromNetwork(request);
            if (cached) return concat(just(std::move(*cached)), network).as_dynamic();
            return network;
    }
}
