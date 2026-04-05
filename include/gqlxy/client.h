#pragma once

#include <gqlxy/cache.h>
#include <gqlxy/link.h>
#include <gqlxy/observable.h>
#include <gqlxy/client/results.h>
#include <memory>

namespace gqlxy {

struct ClientOptions {
    std::shared_ptr<Link> link;
    std::shared_ptr<Cache> cache;
};

class Client {
public:
    Client(const ClientOptions& options);

    Observable<GraphQLResult> Query(const std::string& query, const nlohmann::json& variables = nullptr);
    Observable<GraphQLResult> Mutation(const std::string& query, const nlohmann::json& variables = nullptr);
    Observable<GraphQLResult> Subscribe(const std::string& query, const nlohmann::json& variables = nullptr);

private:
    Observable<GraphQLResult> Execute(const GraphQLRequest& request);

    ClientOptions _options;
};

}
