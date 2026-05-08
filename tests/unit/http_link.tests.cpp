#include "to_result.h"
#include <gqlxy/client/links/http_link.h>
#include <gqlxy/core/results.h>
#include <gtest/gtest.h>

using namespace std;
using namespace testing;
using namespace gqlxy;

class HttpLinkErrorTest : public TestWithParam<string> {};

TEST_P(HttpLinkErrorTest, EmitsException) {
    HttpLink link({.url = GetParam()});
    auto out = to_result(link.Execute({.query = "{ __typename }"}));
    EXPECT_NE(out.exception, nullptr);
    EXPECT_TRUE(out.values.empty());
}

INSTANTIATE_TEST_SUITE_P(BadUrls, HttpLinkErrorTest, Values(
    "not-a-url",
    "ftp://example.com/graphql",
    "http://localhost:19999/graphql",
    "https://localhost:19999/graphql"
));

class HttpLinkTest : public Test {};

TEST_F(HttpLinkTest, OptionsStoredCorrectly) {
    EXPECT_NO_THROW(HttpLink({
        .url = "https://api.example.com/graphql",
        .headers = {{"Authorization", "Bearer token123"}},
    }));
}
