#include "to_result.h"
#include <gqlxy/links/ws_link.h>
#include <gtest/gtest.h>

using namespace std;
using namespace testing;
using namespace gqlxy;

// ─── Parameterized: bad URLs must emit an exception ──────────────────────────

class WsLinkErrorTest : public TestWithParam<string> {};

TEST_P(WsLinkErrorTest, EmitsException) {
    WsLink link({.url = GetParam()});
    auto out = to_result(link.Execute({.query = "{ __typename }"}));
    EXPECT_NE(out.exception, nullptr);
    EXPECT_TRUE(out.values.empty());
}

INSTANTIATE_TEST_SUITE_P(BadUrls, WsLinkErrorTest, Values(
    "not-a-url",
    "ftp://example.com/graphql",
    "http://localhost:19999/graphql", // wrong scheme
    "ws://localhost:19999/graphql"    // unreachable
));

// ─── Construction ─────────────────────────────────────────────────────────────

class WsLinkTest : public Test {};

TEST_F(WsLinkTest, OptionsStoredCorrectly) {
    EXPECT_NO_THROW(WsLink({
        .url = "ws://api.example.com/graphql",
        .headers = {{"Authorization", "Bearer token"}},
    }));
}
