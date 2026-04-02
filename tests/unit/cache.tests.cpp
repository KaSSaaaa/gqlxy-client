#include <gqlxy/cache/in_memory_cache.h>
#include <gtest/gtest.h>

using namespace gqlxy;

TEST(InMemoryCache, MissReturnsNullopt) {
    InMemoryCache cache;
    auto result = cache.Read({.query = "{ __typename }"});
    EXPECT_FALSE(result.has_value());
}

TEST(InMemoryCache, WriteAndRead) {
    InMemoryCache cache;
    GraphQLRequest req{.query = "{ __typename }"};
    GraphQLResult res{.data = nlohmann::json{{"__typename", "Query"}}};
    cache.Write(req, res);
    auto hit = cache.Read(req);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->data, res.data);
}

TEST(InMemoryCache, Evict) {
    InMemoryCache cache;
    GraphQLRequest req{.query = "{ __typename }"};
    cache.Write(req, GraphQLResult{.data = nlohmann::json::object()});
    cache.Evict(req);
    EXPECT_FALSE(cache.Read(req).has_value());
}

TEST(InMemoryCache, VariablesDifferentiateKeys) {
    InMemoryCache cache;
    GraphQLRequest req1{.query = "query($id: ID!) { user(id: $id) { name } }", .variables = {{"id", "1"}}};
    GraphQLRequest req2{.query = "query($id: ID!) { user(id: $id) { name } }", .variables = {{"id", "2"}}};
    cache.Write(req1, GraphQLResult{.data = nlohmann::json{{"user", {{"name", "Alice"}}}}});
    EXPECT_FALSE(cache.Read(req2).has_value());
}
