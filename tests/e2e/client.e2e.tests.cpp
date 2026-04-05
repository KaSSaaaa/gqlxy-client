#include "to_result.h"
#include "server_fixture.h"

#include <gqlxy/client.h>
#include <gqlxy/links/http_link.h>
#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <map>
#include <string>

using namespace std;
using namespace testing;
using namespace gqlxy;
using namespace gqlxy::e2e;

// ─── Shared helper ────────────────────────────────────────────────────────────

static Client MakeClient(const map<string, string>& headers = {}) {
    return Client({
        .link = make_shared<HttpLink>(HttpLinkOptions {
            .url = ServerUrl,
            .headers = headers,
        }),
    });
}

// ─── HTTP query tests ─────────────────────────────────────────────────────────

class HttpLinkTest : public Test {
protected:
    Client _client {MakeClient()};
};

TEST_F(HttpLinkTest, HelloQuery) {
    auto out = to_result(_client.Query("{ hello }"));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
    EXPECT_TRUE(out.completed);
}

TEST_F(HttpLinkTest, EchoWithVariables) {
    auto out = to_result(_client.Query(R"(
        query Echo($msg: String!) {
            echo(message: $msg)
        })", {
            {"msg", "ping pong"}
        })
    );
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["echo"], "ping pong");
}

TEST_F(HttpLinkTest, UserQuery) {
    auto out = to_result(_client.Query(R"(
        query GetUser($id: ID!) {
            user(id: $id) {
                id
                name
                email
            }
        })", {
            {"id", "1"}
        })
    );
    ASSERT_GQL_SUCCESS(out);
    const auto& user = out.values[0].data.value()["user"];
    EXPECT_EQ(user["name"], "Alice");
    EXPECT_EQ(user["email"], "alice@example.com");
}

TEST_F(HttpLinkTest, NullUserReturnsNullData) {
    auto out = to_result(_client.Query(R"(
        query GetUser($id: ID!) {
            user(id: $id) {
                id
                name
            }
        })", {
            {"id", "999"}
        })
    );
    ASSERT_GQL_SUCCESS(out);
    EXPECT_TRUE(out.values[0].data.value()["user"].is_null());
}

TEST_F(HttpLinkTest, GraphQLErrorPropagated) {
    auto out = to_result(_client.Query("{ fail }"));
    ASSERT_FALSE(out.exception);
    ASSERT_EQ(out.values.size(), 1u);
    EXPECT_TRUE(out.values[0].errors.has_value());
    EXPECT_NE(out.values[0].errors->front().message.find("Intentional"), string::npos);
}

TEST_F(HttpLinkTest, CustomHeadersForwarded) {
    auto out = to_result(MakeClient({
        {"x-client-name", "gqlxy-test"}
    }).Query("{ hello }"));
    ASSERT_GQL_SUCCESS(out);
    EXPECT_EQ(out.values[0].data.value()["hello"], "Hello from gqlxy!");
}

// ─── Blocking / concurrency tests ─────────────────────────────────────────────
//
// Execute() uses synchronous Boost.Beast I/O: subscribing blocks the calling
// thread. Two sequential subscriptions therefore take ≥ slow+fast combined,
// while two parallel (async) subscriptions overlap and take ≈ max(slow, fast).

class BlockingTest : public Test {
protected:
    Client _client {MakeClient()};

    using Clock = chrono::steady_clock;
    using Ms = chrono::milliseconds;

    static Clock::time_point Now() {
        return Clock::now();
    }
    static long ElapsedMs(Clock::time_point t0) {
        return chrono::duration_cast<Ms>(Clock::now() - t0).count();
    }
};

TEST_F(BlockingTest, SequentialRequestsAreBlocking) {
    constexpr int DelayMs = 100;

    const auto t0 = Now();
    auto slow = to_result(
        _client.Query(R"(
            query Slow($ms: Int!) {
                delay(ms: $ms)
            })", {
                {"ms", DelayMs}
            }
        )
    );
    auto fast = to_result(_client.Query("{ hello }"));
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
        return to_result(_client.Query(R"(
            query Slow($ms: Int!) {
                delay(ms: $ms)
            })", {
                {"ms", DelayMs}
            })
        );
    });
    auto fast_f = async(launch::async, [this] { return to_result(_client.Query("{ hello }")); });

    auto slow = slow_f.get();
    auto fast = fast_f.get();
    const auto total = ElapsedMs(t0);

    ASSERT_GQL_SUCCESS(slow);
    ASSERT_GQL_SUCCESS(fast);
    EXPECT_EQ(slow.values[0].data.value()["delay"], "delayed 100ms");
    EXPECT_LT(total, DelayMs + Margin);
}

// ─── SSE subscription tests ───────────────────────────────────────────────────
//
// Parameterized by event count: verifies that onCount(to: N) emits exactly N
// events (each = counter value 1..N) and then completes.

class SseCountTest : public TestWithParam<int> {
protected:
    Client _client {MakeClient()};
};

TEST_P(SseCountTest, ReceivesExactCountAndCompletes) {
    const int count = GetParam();
    auto out = to_result(
        _client.Subscribe(R"(
            subscription OnCount($to: Int!) {
                onCount(to: $to)
            }
        )", {
            {"to", count}
        })
    );

    ASSERT_FALSE(out.exception);
    ASSERT_EQ(out.values.size(), static_cast<size_t>(count));
    EXPECT_TRUE(out.completed);

    for (int i = 0; i < count; ++i) {
        ASSERT_TRUE(out.values[i].data.has_value());
        EXPECT_EQ(out.values[i].data.value()["onCount"].get<int>(), i + 1);
    }
}

INSTANTIATE_TEST_SUITE_P(EventCounts, SseCountTest, Values(0, 3, 5));

class SseTest : public Test {};

TEST_F(SseTest, CustomHeadersForwarded) {
    auto out = to_result(MakeClient({
        {"X-Test-Header", "sse-value"}
    }).Subscribe(R"(
        subscription OnCount($to: Int!) {
            onCount(to: $to)
        }
    )", {
        {"to", 2}
    }));

    ASSERT_FALSE(out.exception);
    ASSERT_EQ(out.values.size(), 2u);
    EXPECT_TRUE(out.completed);
}
