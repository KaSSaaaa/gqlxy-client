#include <gqlxy/cache/in_memory_cache.h>
#include <gqlxy/client.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/split_link.h>
#include <gqlxy/links/ws_link.h>
#include <gqlxy/task.h>

#include <iostream>

using namespace std;
using namespace gqlxy;

// co_await style: Task<T> is the coroutine return type; Observable<T> is awaitable inside it.
Task<int> run(Client& client) {
    auto result = co_await client.Query(R"( query { __typename } )");

    if (result.data)
        cout << result.data->dump(2) << endl;
    else if (result.errors)
        for (const auto& e : *result.errors) cerr << "Error: " << e.message << endl;

    co_return 0;
}

int main() {
    // clang-format off
    Client client({
        .link = make_shared<SplitLink>(
            [](const GraphQLRequest& req) {
                return req.query.find("subscription") == string::npos;
            },
            make_shared<HttpLink>(HttpLinkOptions{.url = "http://localhost:4000/graphql"}),
            make_shared<WsLink>(WsLinkOptions{.url = "ws://localhost:4000/graphql"})
        ),
        .cache = make_shared<InMemoryCache>()
    });
    // clang-format on

    // Coroutine style — run() uses co_await internally, .get() resolves synchronously here
    run(client).get();

    // Subscribe style — query
    client.Query(R"( query { __typename } )")
        .subscribe(
            [](const GraphQLResult& r) { cout << r.data->dump(2) << endl; },
            [](exception_ptr) { cerr << "Query error" << endl; }
        );

    // Subscribe style — subscription (streams until unsubscribed)
    auto sub = client.Subscribe(R"( subscription { onMessage { text } } )")
        .subscribe(
            [](const GraphQLResult& r) { cout << r.data->dump(2) << endl; },
            [](exception_ptr) { cerr << "Subscription error" << endl; }
        );

    return 0;
}
