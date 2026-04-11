#include "server_fixture.h"
#include "to_result.h"

#include <gqlxy/cache/in_memory_cache.h>
#include <gqlxy/client.h>
#include <gqlxy/client/fetch_policy.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/split_link.h>
#include <gqlxy/links/ws_link.h>
#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <future>
#include <string>
#include <tuple>

using namespace std;
using namespace testing;
using namespace gqlxy;
using namespace gqlxy::e2e;

struct LinkParam {
    string name;
    function<Client()> factory;

    friend std::ostream& operator<<(std::ostream& os, const LinkParam& obj) {
        return os << obj.name;
    }
};

// clang-format off
static const auto TestCaCert = std::string(CaCert);

static const LinkParam HttpParam {
    "Http",
    [] { return Client({.link = make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl})}); }
};

static const LinkParam HttpsParam {
    "Https",
    [] { return Client({.link = make_shared<HttpLink>(HttpLinkOptions{.url = HttpsServerUrl, .caCert = TestCaCert})}); }
};

static const LinkParam WsParam {
    "Ws",
    [] { return Client({.link = make_shared<WsLink>(WsLinkOptions{.url = WsServerUrl})}); }
};

static const LinkParam WssParam {
    "Wss",
    [] { return Client({.link = make_shared<WsLink>(WsLinkOptions{.url = WssServerUrl, .caCert = TestCaCert})}); }
};

static const LinkParam SplitParam {
    "Split",
    [] {
        return Client({
            .link = make_shared<SplitLink>(
                [](const GraphQLRequest& req) { return req.type != OperationType::Subscription; },
                make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl}),
                make_shared<WsLink>(WsLinkOptions{.url = WsServerUrl})
            )
        });
    }
};

static const LinkParam SplitSslParam {
    "SplitSsl",
    [] {
        return Client({
            .link = make_shared<SplitLink>(
                [](const GraphQLRequest& req) { return req.type != OperationType::Subscription; },
                make_shared<HttpLink>(HttpLinkOptions{.url = HttpsServerUrl, .caCert = TestCaCert}),
                make_shared<WsLink>(WsLinkOptions{.url = WssServerUrl, .caCert = TestCaCert})
            )
        });
    }
};
// clang-format on

class LinkTest : public TestWithParam<LinkParam> {
protected:
    optional<Client> _client;
    void SetUp() override { _client.emplace(GetParam().factory()); }
};

TEST_P(LinkTest, HelloQuery) {
    auto out = to_result(_client->Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
    EXPECT_TRUE(out.completed);
}

TEST_P(LinkTest, EchoWithVariables) {
    auto out = to_result(_client->Query({
        .query = R"(
        query Echo($msg: String!) {
            echo(message: $msg)
        })",
        .variables = {{"msg", "ping pong"}},
    }));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["echo"], "ping pong");
}

TEST_P(LinkTest, UserQuery) {
    auto out = to_result(_client->Query({
        .query = R"(
        query GetUser($id: ID!) {
            user(id: $id) {
                id
                name
                email
            }
        })",
        .variables = {{"id", "1"}},
    }));
    ASSERT_GQL_SUCCESS(out);
    const auto& user = out.values[0].data.value()["user"];
    EXPECT_EQ(user["name"], "Alice");
    EXPECT_EQ(user["email"], "alice@example.com");
}

TEST_P(LinkTest, NullUserReturnsNullData) {
    auto out = to_result(_client->Query({
        .query = R"(
        query GetUser($id: ID!) {
            user(id: $id) {
                id
                name
            }
        })",
        .variables = {{"id", "999"}},
    }));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_TRUE(out.values[0].data.value()["user"].is_null());
}

TEST_P(LinkTest, GraphQLErrorPropagated) {
    auto out = to_result(_client->Query({.query = "{ fail }"}));
    ASSERT_FALSE(out.exception);
    ASSERT_EQ(out.values.size(), 1u);
    EXPECT_TRUE(out.values[0].errors.has_value());
    EXPECT_NE(out.values[0].errors->front().message.find("Intentional"), string::npos);
}

INSTANTIATE_TEST_SUITE_P(
    Links, LinkTest,
    Values(HttpParam, HttpsParam, WsParam, WssParam, SplitParam, SplitSslParam),
    [](const TestParamInfo<LinkParam>& info) { return info.param.name; });

class CountTest : public TestWithParam<tuple<LinkParam, int>> {
protected:
    optional<Client> _client;
    void SetUp() override { _client.emplace(get<0>(GetParam()).factory()); }
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
    LinkXCount, CountTest,
    Combine(Values(HttpParam, HttpsParam, WsParam, WssParam, SplitParam, SplitSslParam), Values(0, 3, 5)),
    [](const TestParamInfo<tuple<LinkParam, int>>& info) {
        return get<0>(info.param).name + "_" + to_string(get<1>(info.param));
    });

TEST(HttpLinkTest, CustomHeadersForwarded) {
    Client client({.link = make_shared<HttpLink>(HttpLinkOptions{
        .url = ServerUrl,
        .headers = {{"x-client-name", "gqlxy-test"}},
    })});
    auto out = to_result(client.Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
}

TEST(SseTest, CustomHeadersForwarded) {
    Client client({.link = make_shared<HttpLink>(HttpLinkOptions{
        .url = ServerUrl,
        .headers = {{"X-Test-Header", "sse-value"}},
    })});
    auto out = to_result(client.Subscribe({
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

class BlockingTest : public Test {
protected:
    Client _client {Client({.link = make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl})})};

    using Clock = chrono::steady_clock;
    using Ms = chrono::milliseconds;

    static Clock::time_point Now() { return Clock::now(); }
    static long ElapsedMs(Clock::time_point t0) {
        return chrono::duration_cast<Ms>(Clock::now() - t0).count();
    }
};

TEST_F(BlockingTest, SequentialRequestsAreBlocking) {
    constexpr int DelayMs = 100;

    const auto t0 = Now();
    auto slow = to_result(_client.Query({
        .query = R"(
        query Slow($ms: Int!) {
            delay(ms: $ms)
        })",
        .variables = {{"ms", DelayMs}},
    }));
    auto fast = to_result(_client.Query({.query = "{ hello }"}));
    const auto total = ElapsedMs(t0);

    ASSERT_GQL_SUCCESS(slow);
    ASSERT_GQL_SUCCESS(fast);
    EXPECT_EQ(slow.values[0].data.value()["delay"], "delayed 100ms");
    EXPECT_GE(total, DelayMs);
}

TEST_F(BlockingTest, ParallelRequestsOverlap) {
    constexpr int DelayMs = 100;
    constexpr int Margin = 60;

    const auto t0 = Now();
    auto slow_f = async(launch::async, [DelayMs, this] {
        return to_result(_client.Query({
            .query = R"(
            query Slow($ms: Int!) {
                delay(ms: $ms)
            })",
            .variables = {{"ms", DelayMs}},
        }));
    });
    auto fast_f = async(launch::async, [this] { return to_result(_client.Query({.query = "{ hello }"})); });

    auto slow = slow_f.get();
    auto fast = fast_f.get();
    const auto total = ElapsedMs(t0);

    ASSERT_GQL_SUCCESS(slow);
    ASSERT_GQL_SUCCESS(fast);
    EXPECT_EQ(slow.values[0].data.value()["delay"], "delayed 100ms");
    EXPECT_LT(total, DelayMs + Margin);
}

class WsLinkPersistenceTest : public Test {
protected:
    Client _client {Client({.link = make_shared<WsLink>(WsLinkOptions{.url = WsServerUrl})})};
};

TEST_F(WsLinkPersistenceTest, ConnectionReused) {
    for (int i = 0; i < 3; ++i) {
        auto out = to_result(_client.Query({.query = "{ hello }"}));
        ASSERT_GQL_SUCCESS(out);
        EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
    }
}

TEST_F(WsLinkPersistenceTest, ConcurrentSubscriptions) {
    auto sub_f = std::async(std::launch::async, [this] {
        return to_result(_client.Subscribe({
            .query = R"(
            subscription OnCount($to: Int!) {
                onCount(to: $to)
            }
        )",
            .variables = {{"to", 3}},
        }));
    });
    auto query_f = std::async(std::launch::async, [this] {
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
            .caCert = TestCaCert,
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
                .caCert = TestCaCert,
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
    auto sub_out = std::async(std::launch::async, [this] {
        return to_result(_client.Subscribe({
            .query = R"(
            subscription OnCount($to: Int!) {
                onCount(to: $to)
            }
        )",
            .variables = {{"to", 3}},
        }));
    }).get();
    auto query_out = std::async(std::launch::async, [this] {
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

    Observable<GraphQLResult> Execute(const GraphQLRequest& request) override {
        _callCount++;
        return _inner->Execute(request);
    }

    int CallCount() const { return _callCount; }

private:
    shared_ptr<Link> _inner;
    atomic<int> _callCount {0};
};

TEST(CachePolicyTest, CacheFirstSkipsNetworkOnHit) {
    auto spy = make_shared<SpyLink>(make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl}));
    auto cache = make_shared<InMemoryCache>();
    Client client({.link = spy, .cache = cache});

    auto out1 = to_result(client.Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out1);
    EXPECT_EQ(spy->CallCount(), 1);

    auto out2 = to_result(client.Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out2);
    EXPECT_EQ(spy->CallCount(), 1);
    EXPECT_EQ(out2.values[0].data, out1.values[0].data);
}

TEST(CachePolicyTest, NetworkOnlyAlwaysFetches) {
    auto spy = make_shared<SpyLink>(make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl}));
    auto cache = make_shared<InMemoryCache>();
    Client client({.link = spy, .cache = cache});

    auto out1 = to_result(client.Query({.query = "{ hello }", .fetchPolicy = FetchPolicy::NetworkOnly}));
    ASSERT_GQL_SUCCESS(out1);

    auto out2 = to_result(client.Query({.query = "{ hello }", .fetchPolicy = FetchPolicy::NetworkOnly}));
    ASSERT_GQL_SUCCESS(out2);
    EXPECT_EQ(spy->CallCount(), 2);
}

TEST(CachePolicyTest, CacheAndNetworkEmitsTwoResults) {
    auto spy = make_shared<SpyLink>(make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl}));
    auto cache = make_shared<InMemoryCache>();
    Client client({.link = spy, .cache = cache});

    to_result(client.Query({.query = "{ hello }"}));

    auto out = to_result(client.Query({.query = "{ hello }", .fetchPolicy = FetchPolicy::CacheAndNetwork}));
    ASSERT_FALSE(out.exception);
    EXPECT_EQ(out.values.size(), 2u);
    EXPECT_EQ(spy->CallCount(), 2);
}

TEST(CachePolicyTest, NoCacheDoesNotPopulateCache) {
    auto spy = make_shared<SpyLink>(make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl}));
    auto cache = make_shared<InMemoryCache>();
    Client client({.link = spy, .cache = cache});

    to_result(client.Query({.query = "{ hello }", .fetchPolicy = FetchPolicy::NoCache}));

    auto out = to_result(client.Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(spy->CallCount(), 2);
}

TEST(CachePolicyTest, RefetchBypassesCache) {
    auto spy = make_shared<SpyLink>(make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl}));
    auto cache = make_shared<InMemoryCache>();
    Client client({.link = spy, .cache = cache});

    to_result(client.Query({.query = "{ hello }"}));
    EXPECT_EQ(spy->CallCount(), 1);

    auto out = to_result(client.Refetch({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(spy->CallCount(), 2);
}

TEST(CachePolicyTest, MutationUsesNetworkOnly) {
    auto spy = make_shared<SpyLink>(make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl}));
    auto cache = make_shared<InMemoryCache>();
    Client client({.link = spy, .cache = cache});

    auto out = to_result(client.Mutation({
        .query = R"(
        mutation Echo($msg: String!) {
            echo(message: $msg)
        }
    )",
        .variables = {{"msg", "test"}},
    }));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(spy->CallCount(), 1);
}

TEST(CachePolicyTest, NoCacheClientWorks) {
    Client client({.link = make_shared<HttpLink>(HttpLinkOptions{.url = ServerUrl})});

    auto out = to_result(client.Query({.query = "{ hello }"}));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
}
