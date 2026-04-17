#include <gqlxy/cache/in_memory_cache.h>
#include <gqlxy/client.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/split_link.h>
#include <gqlxy/links/ws_link.h>

#include <iostream>

using namespace std;
using namespace gqlxy;

int main() {
    // clang-format off
    Client client({
        .link = make_shared<SplitLink>(
            [](const auto& req) { return req.type != OperationType::Subscription; },
            make_shared<HttpLink>(HttpLinkOptions{.url = "http://localhost:4000/graphql"}),
            make_shared<WsLink>(WsLinkOptions{.url = "ws://localhost:4000/graphql"})
        ),
        .cache = make_shared<InMemoryCache>()
    });
    // clang-format on

    client.Query({
        .query = R"(
            query {
                __typename
            }
        )"
    })
        .subscribe(
            [](const auto& r) {
                auto [data, errors] = r;

                if (data) cout << r.data->dump(2) << endl;
                if (errors)
                    for (const auto& [message, _] : *r.errors)
                        cerr << "Error: " << message << endl;
            },
            [](auto) { cerr << "Query error" << endl; }
        );

    auto sub = client.Subscribe({
        .query = R"(
            subscription {
                onMessage {
                    text
                }
            }
        )"})
        .subscribe(
            [](const auto& r) { cout << r.data->dump(2) << endl; },
            [](auto) { cerr << "Subscription error" << endl; }
        );

    string input;
    getline(cin, input);

    return 0;
}
