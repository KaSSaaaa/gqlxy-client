#include "server_fixture.h"
#include "to_result.h"
#include <chrono>
#include <functional>
#include <future>
#include <gqlxy/cache/in_memory_cache.h>
#include <gqlxy/client.h>
#include <gqlxy/client/fetch_policy.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/split_link.h>
#include <gqlxy/links/ws_link.h>
#include <gqlxy/results.h>
#include <gtest/gtest.h>
#include <string>
#include <tuple>

using namespace std;
using namespace std::chrono;
using namespace testing;
using namespace gqlxy;
using namespace gqlxy::parser;
using namespace gqlxy::e2e;

struct LinkParam {
    string name;
    function<Client()> factory;

    friend ostream& operator<<(ostream& os, const LinkParam& obj) {
        return os << obj.name;
    }
};

// clang-format off
static const vector<LinkParam> LinkParams {
    {"Http", [] { return Client({.link = make_shared<HttpLink>(HttpLinkOptions {.url = ServerUrl})}); }},
    {"Https", [] {return Client({.link = make_shared<HttpLink>(HttpLinkOptions {.url = HttpsServerUrl, .caCert = CaCert})}); }},
    {"Ws", [] { return Client({.link = make_shared<WsLink>(WsLinkOptions {.url = WsServerUrl})}); }},
    {"Wss", [] { return Client({.link = make_shared<WsLink>(WsLinkOptions {.url = WssServerUrl, .caCert = CaCert})}); }},
    {"Split",[] {
        return Client({
            .link = make_shared<SplitLink>(
                [](const GraphQLRequest& req) { return req.type._value != OperationType::SUBSCRIPTION; },
                make_shared<HttpLink>(HttpLinkOptions {.url = ServerUrl}),
                make_shared<WsLink>(WsLinkOptions {.url = WsServerUrl})
            )
        });
    }},
    {"SplitSsl", [] {
        return Client({
            .link = make_shared<SplitLink>(
                [](const GraphQLRequest& req) { return req.type._value != OperationType::SUBSCRIPTION; },
                make_shared<HttpLink>(HttpLinkOptions {.url = HttpsServerUrl, .caCert = CaCert}),
                make_shared<WsLink>(WsLinkOptions {.url = WssServerUrl, .caCert = CaCert})
            )
        });
    }}
};
// clang-format on

class LinkParamTest : public TestWithParam<LinkParam> {
protected:
    unique_ptr<Client> _client;

    void SetUp() override {
        _client = make_unique<Client>(GetParam().factory());
    }
};

TEST_P(LinkParamTest, HelloQuery) {
    auto out = to_result(_client->Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
    EXPECT_TRUE(out.completed);
}

TEST_P(LinkParamTest, EchoWithVariables) {
    auto out = to_result(_client->Query({
        .query = R"(
            query Echo($msg: String!) {
                echo(message: $msg)
            }
        )",
        .variables = {{"msg", "ping pong"}},
    }));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["echo"], "ping pong");
}

TEST_P(LinkParamTest, UserQuery) {
    auto out = to_result(_client->Query({
        .query = R"(
            query GetUser($id: ID!) {
                user(id: $id) {
                    id
                    name
                    email
                }
            }
        )",
        .variables = {{"id", "1"}},
    }));
    ASSERT_GQL_SUCCESS(out);
    const auto& user = out.values[0].data.value()["user"];
    EXPECT_EQ(user["name"], "Alice");
    EXPECT_EQ(user["email"], "alice@example.com");
}

TEST_P(LinkParamTest, NullUserReturnsNullData) {
    auto out = to_result(_client->Query({
        .query = R"(
            query GetUser($id: ID!) {
                user(id: $id) {
                    id
                    name
                }
            }
        )",
        .variables = {{"id", "999"}},
    }));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_TRUE(out.values[0].data.value()["user"].is_null());
}

TEST_P(LinkParamTest, GraphQLErrorPropagated) {
    auto out = to_result(_client->Query({.query = "{ fail }"}));
    ASSERT_FALSE(out.exception);
    ASSERT_EQ(out.values.size(), 1u);
    EXPECT_TRUE(out.values[0].errors.has_value());
    EXPECT_NE(out.values[0].errors->front().message.find("Intentional"), string::npos);
}

INSTANTIATE_TEST_SUITE_P(Links, LinkParamTest, ValuesIn(LinkParams), [](const TestParamInfo<LinkParam>& info) {
    return info.param.name;
});

class CountTest : public TestWithParam<tuple<LinkParam, int>> {
protected:
    unique_ptr<Client> _client;

    void SetUp() override {
        _client = make_unique<Client>(get<0>(GetParam()).factory());
    }
};

TEST_P(CountTest, ReceivesExactCountAndCompletes) {
    const int count = get<1>(GetParam());
    auto out = to_result(_client->Subscribe({
        .query = R"(
            subscription OnCount($to: Int!) {
                onCount(to: $to)
            }
        )",
        .variables = {{"to", count}},
    }));

    ASSERT_FALSE(out.exception);
    ASSERT_EQ(out.values.size(), static_cast<size_t>(count));
    EXPECT_TRUE(out.completed);

    for (int i = 0; i < count; ++i) {
        ASSERT_TRUE(out.values[i].data.has_value());
        EXPECT_EQ(out.values[i].data.value()["onCount"].get<int>(), i + 1);
    }
}

INSTANTIATE_TEST_SUITE_P(
    LinkXCount, CountTest, Combine(ValuesIn(LinkParams), Values(0, 3, 5)),
    [](const TestParamInfo<tuple<LinkParam, int>>& info) {
        return get<0>(info.param).name + "_" + to_string(get<1>(info.param));
    });

class LinkTests : public Test {
protected:
    unique_ptr<Client> _client {
        make_unique<Client>(Client({
            .link = make_shared<HttpLink>(HttpLinkOptions{
                .url = ServerUrl,
                .headers = {{"x-client-name", "gqlxy-test"}},
            })
        }))
    };

    static steady_clock::time_point Now() { return steady_clock::now(); }
    static long ElapsedMs(steady_clock::time_point t0) {
        return chrono::duration_cast<milliseconds>(steady_clock::now() - t0).count();
    }
};

TEST_F(LinkTests, Query_CustomHeadersForwarded) {
    auto out = to_result(_client->Query({.query = "{ hello }"}));

    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
}

TEST_F(LinkTests, Sse_CustomHeadersForwarded) {
    auto out = to_result(_client->Subscribe({
        .query = R"(
            subscription OnCount($to: Int!) {
                onCount(to: $to)
            }
        )",
        .variables = {{"to", 2}},
    }));

    ASSERT_FALSE(out.exception);
    ASSERT_EQ(out.values.size(), 2u);
    EXPECT_TRUE(out.completed);
}

TEST_F(LinkTests, SequentialRequestsAreBlocking) {
    auto delayMs = 100;

    const auto t0 = Now();
    auto slow = to_result(_client->Query({
        .query = R"(
            query Slow($ms: Int!) {
                delay(ms: $ms)
            }
        )",
        .variables = {{"ms", delayMs}},
    }));
    auto fast = to_result(_client->Query({.query = "{ hello }"}));
    const auto total = ElapsedMs(t0);

    ASSERT_GQL_SUCCESS(slow);
    ASSERT_GQL_SUCCESS(fast);
    EXPECT_EQ(slow.values[0].data.value()["delay"], "delayed 100ms");
    EXPECT_GE(total, delayMs);
}

TEST_F(LinkTests, ParallelRequestsOverlap) {
    auto delayMs = 500;

    const auto slowQuery = R"(
        query Slow($ms: Int!) {
            delay(ms: $ms)
        }
    )";

    const auto sequential = Now();
    auto sequentialSlow = to_result(_client->Query({.query = slowQuery, .variables = {{"ms", delayMs}}}));
    auto sequentialFast = to_result(_client->Query({.query = "{ hello }"}));
    const auto sequentialMs = ElapsedMs(sequential);

    ASSERT_GQL_SUCCESS(sequentialSlow);
    ASSERT_GQL_SUCCESS(sequentialFast);

    const auto parallel = Now();
    auto slowFunction = async(launch::async, [&] {
        return to_result(_client->Query({.query = slowQuery, .variables = {{"ms", delayMs}}}));
    });
    auto fastFunction = async(launch::async, [&] {
        return to_result(_client->Query({.query = "{ hello }"}));
    });
    auto parallelSlow = slowFunction.get();
    auto parallelFast = fastFunction.get();
    const auto parallelMs = ElapsedMs(parallel);

    ASSERT_GQL_SUCCESS(parallelSlow);
    ASSERT_GQL_SUCCESS(parallelFast);
    EXPECT_EQ(parallelSlow.values[0].data.value()["delay"], "delayed 500ms");

    EXPECT_LT(parallelMs, sequentialMs);
}

class WsLinkPersistenceTest : public Test {
protected:
    Client _client {
        Client({
            .link = make_shared<WsLink>(WsLinkOptions{
                .url = WsServerUrl
            })
        })
    };
};

TEST_F(WsLinkPersistenceTest, ConnectionReused) {
    for (int i = 0; i < 3; ++i) {
        auto out = to_result(_client.Query({.query = "{ hello }"}));
        ASSERT_GQL_SUCCESS(out);
        EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
    }
}

TEST_F(WsLinkPersistenceTest, ConcurrentSubscriptions) {
    auto sub_f = async(launch::async, [this] {
        return to_result(_client.Subscribe({
            .query = R"(
                subscription OnCount($to: Int!) {
                    onCount(to: $to)
                }
            )",
            .variables = {{"to", 3}},
        }));
    });
    auto query_f = async(launch::async, [this] {
        return to_result(_client.Query({.query = "{ hello }"}));
    });

    auto sub_out = sub_f.get();
    auto query_out = query_f.get();

    ASSERT_GQL_SUCCESS(query_out);
    ASSERT_FALSE(sub_out.exception);
    ASSERT_EQ(sub_out.values.size(), 3u);
    EXPECT_TRUE(sub_out.completed);
}

TEST(HttpsLinkTest, CustomHeadersForwarded) {
    Client client({
        .link = make_shared<HttpLink>(HttpLinkOptions{
            .url = HttpsServerUrl,
            .headers = {{"x-client-name", "gqlxy-ssl-test"}},
            .caCert = CaCert,
        })
    });
    auto out = to_result(client.Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
}

class WssLinkPersistenceTest : public Test {
protected:
    Client _client {
        Client({
            .link = make_shared<WsLink>(WsLinkOptions {
                .url = WssServerUrl,
                .caCert = CaCert,
            })
        })
    };
};

TEST_F(WssLinkPersistenceTest, ConnectionReused) {
    for (int i = 0; i < 3; ++i) {
        auto out = to_result(_client.Query({.query = "{ hello }"}));
        ASSERT_GQL_SUCCESS(out);
        EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
    }
}

TEST_F(WssLinkPersistenceTest, ConcurrentSubscriptions) {
    auto sub_out = async(launch::async, [this] {
        return to_result(_client.Subscribe({
            .query = R"(
                subscription OnCount($to: Int!) {
                    onCount(to: $to)
                }
            )",
            .variables = {{"to", 3}},
        }));
    }).get();
    auto query_out = async(launch::async, [this] {
        return to_result(_client.Query({.query = "{ hello }"}));
    }).get();

    ASSERT_GQL_SUCCESS(query_out);
    ASSERT_FALSE(sub_out.exception);
    ASSERT_EQ(sub_out.values.size(), 3u);
    EXPECT_TRUE(sub_out.completed);
}

TEST(SslVerificationTest, RejectsUntrustedCert) {
    WsLink link({.url = WssServerUrl});
    auto out = to_result(link.Execute({.query = "{ __typename }"}));
    EXPECT_NE(out.exception, nullptr);
}

class SpyLink : public Link {
public:
    explicit SpyLink(shared_ptr<Link> inner) : _inner(std::move(inner)) {}

    Observable<GraphQLResponse> Execute(const GraphQLRequest& request) override {
        ++_callCount;
        return _inner->Execute(request);
    }

    int CallCount() const { return _callCount; }

private:
    shared_ptr<Link> _inner;
    atomic<int> _callCount {0};
};

class CachePolicyTest : public Test {
protected:
    shared_ptr<SpyLink> _spy {
        make_shared<SpyLink>(make_shared<HttpLink>(HttpLinkOptions{
            .url = ServerUrl
        }))
    };
    shared_ptr<InMemoryCache> _cache { make_shared<InMemoryCache>() };
    Client _client {
        {
            .link = _spy,
            .cache = _cache
        }
    };
};

TEST_F(CachePolicyTest, CacheFirstSkipsNetworkOnHit) {
    auto out1 = to_result(_client.Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out1);
    EXPECT_EQ(_spy->CallCount(), 1);

    auto out2 = to_result(_client.Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out2);
    EXPECT_EQ(_spy->CallCount(), 1);
    EXPECT_EQ(out2.values[0].data, out1.values[0].data);
}

TEST_F(CachePolicyTest, NetworkOnlyAlwaysFetches) {
    auto out1 = to_result(_client.Query({
        .query = "{ hello }",
        .fetchPolicy = FetchPolicy::NetworkOnly
    }));
    ASSERT_GQL_SUCCESS(out1);

    auto out2 = to_result(_client.Query({
        .query = "{ hello }",
        .fetchPolicy = FetchPolicy::NetworkOnly
    }));
    ASSERT_GQL_SUCCESS(out2);
    EXPECT_EQ(_spy->CallCount(), 2);
}

TEST_F(CachePolicyTest, CacheAndNetworkEmitsTwoResults) {
    to_result(_client.Query({.query = "{ hello }"}));

    auto out = to_result(_client.Query({.query = "{ hello }", .fetchPolicy = FetchPolicy::CacheAndNetwork}));
    ASSERT_FALSE(out.exception);
    EXPECT_EQ(out.values.size(), 2u);
    EXPECT_EQ(_spy->CallCount(), 2);
}

TEST_F(CachePolicyTest, NoCacheDoesNotPopulateCache) {
    to_result(_client.Query({.query = "{ hello }", .fetchPolicy = FetchPolicy::NoCache}));

    auto out = to_result(_client.Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(_spy->CallCount(), 2);
}

TEST_F(CachePolicyTest, RefetchBypassesCache) {
    to_result(_client.Query({.query = "{ hello }"}));
    EXPECT_EQ(_spy->CallCount(), 1);

    auto out = to_result(_client.Refetch({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(_spy->CallCount(), 2);
}

TEST_F(CachePolicyTest, MutationUsesNetworkOnly) {
    auto out = to_result(_client.Mutation({
        .query = R"(
            mutation Echo($msg: String!) {
                echo(message: $msg)
            }
        )",
        .variables = {{"msg", "test"}},
    }));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(_spy->CallCount(), 1);
}

TEST_F(CachePolicyTest, NoCacheClientWorks) {
    Client client({.link = make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl})});

    auto out = to_result(client.Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
}
