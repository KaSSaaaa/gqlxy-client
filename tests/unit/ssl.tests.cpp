#include "to_result.h"
#include <gqlxy/internal/url.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/ws_link.h>
#include <gtest/gtest.h>

using namespace std;
using namespace testing;
using namespace gqlxy;
using namespace gqlxy::internal;

class UrlParsingTest : public Test {};

TEST_F(UrlParsingTest, HttpIsNotTls) {
    auto url = ParseHttpUrl("http://example.com/graphql");
    EXPECT_FALSE(url.tls);
    EXPECT_EQ(url.port, "80");
}

TEST_F(UrlParsingTest, HttpsIsTls) {
    auto url = ParseHttpUrl("https://example.com/graphql");
    EXPECT_TRUE(url.tls);
    EXPECT_EQ(url.port, "443");
}

TEST_F(UrlParsingTest, WsIsNotTls) {
    auto url = ParseWsUrl("ws://example.com/graphql");
    EXPECT_FALSE(url.tls);
    EXPECT_EQ(url.port, "80");
}

TEST_F(UrlParsingTest, WssIsTls) {
    auto url = ParseWsUrl("wss://example.com/graphql");
    EXPECT_TRUE(url.tls);
    EXPECT_EQ(url.port, "443");
}

TEST_F(UrlParsingTest, CustomPortPreserved) {
    auto url = ParseHttpUrl("https://example.com:8443/graphql");
    EXPECT_TRUE(url.tls);
    EXPECT_EQ(url.port, "8443");
    EXPECT_EQ(url.host, "example.com");
    EXPECT_EQ(url.target, "/graphql");
}

TEST_F(UrlParsingTest, WssCustomPortPreserved) {
    auto url = ParseWsUrl("wss://example.com:9443/sub");
    EXPECT_TRUE(url.tls);
    EXPECT_EQ(url.port, "9443");
    EXPECT_EQ(url.target, "/sub");
}

class HttpsLinkTest : public Test {};

TEST_F(HttpsLinkTest, ConstructsWithHttpsUrl) {
    EXPECT_NO_THROW(HttpLink({.url = "https://api.example.com/graphql"}));
}

TEST_F(HttpsLinkTest, UnreachableEmitsException) {
    HttpLink link({.url = "https://localhost:19999/graphql"});
    auto out = to_result(link.Execute({.query = "{ __typename }"}));
    EXPECT_NE(out.exception, nullptr);
    EXPECT_TRUE(out.values.empty());
}

class WssLinkTest : public Test {};

TEST_F(WssLinkTest, ConstructsWithWssUrl) {
    EXPECT_NO_THROW(WsLink({.url = "wss://api.example.com/graphql"}));
}

TEST_F(WssLinkTest, UnreachableEmitsException) {
    WsLink link({.url = "wss://localhost:19999/graphql"});
    auto out = to_result(link.Execute({.query = "{ __typename }"}));
    EXPECT_NE(out.exception, nullptr);
    EXPECT_TRUE(out.values.empty());
}
