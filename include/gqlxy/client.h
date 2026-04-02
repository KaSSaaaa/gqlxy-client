#pragma once

#include <gqlxy/cache.h>
#include <gqlxy/link.h>
#include <gqlxy/observable.h>
#include <gqlxy/results.h>
#include <memory>
#include <string_view>

namespace gqlxy {

struct ClientOptions {
    std::shared_ptr<Link> link;
    std::shared_ptr<Cache> cache;
};

class Client {
public:
    explicit Client(ClientOptions options);

    // All three return an Observable<GraphQLResult> that can be:
    //   - co_await-ed  → resolves the first emitted value
    //   - .subscribe() → streams all values (use this for Subscribe)
    Observable<GraphQLResult> Query(std::string_view query, nlohmann::json variables = nullptr);
    Observable<GraphQLResult> Mutation(std::string_view query, nlohmann::json variables = nullptr);
    Observable<GraphQLResult> Subscribe(std::string_view query, nlohmann::json variables = nullptr);

private:
    Observable<GraphQLResult> Execute(GraphQLRequest request);

    ClientOptions options_;
};

} // namespace gqlxy
