#include "to_result.h"
#include <gqlxy/cache/in_memory_cache.h>
#include <gtest/gtest.h>

using namespace gqlxy;

class InMemoryCacheTest : public ::testing::Test {
protected:
    InMemoryCache _cache;
};

TEST_F(InMemoryCacheTest, MissReturnsNullopt) {
    EXPECT_FALSE(_cache.Read({.query = "{ __typename }"}).has_value());
}

TEST_F(InMemoryCacheTest, WriteAndRead) {
    GraphQLRequest req {.query = "{ __typename }"};
    GraphQLResult res {.data = nlohmann::json {{"__typename", "Query"}}};
    _cache.Write(req, res);
    auto hit = _cache.Read(req);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->data, res.data);
}

TEST_F(InMemoryCacheTest, Evict) {
    GraphQLRequest req {.query = "{ __typename }"};
    _cache.Write(req, GraphQLResult {.data = nlohmann::json::object()});
    _cache.Evict(req);
    EXPECT_FALSE(_cache.Read(req).has_value());
}

TEST_F(InMemoryCacheTest, VariablesDifferentiateKeys) {
    GraphQLRequest req1 {.query = "query($id: ID!) { user(id: $id) { name } }", .variables = {{"id", "1"}}};
    GraphQLRequest req2 {.query = "query($id: ID!) { user(id: $id) { name } }", .variables = {{"id", "2"}}};
    _cache.Write(req1, GraphQLResult {.data = nlohmann::json {{"user", {{"name", "Alice"}}}}});
    EXPECT_FALSE(_cache.Read(req2).has_value());
}
