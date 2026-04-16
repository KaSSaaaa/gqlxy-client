#include <gqlxy/cache/in_memory_cache.h>
#include <gtest/gtest.h>

using namespace gqlxy;
using namespace nlohmann;

class InMemoryCacheTest : public ::testing::Test {
protected:
    InMemoryCache _cache;
};

TEST_F(InMemoryCacheTest, MissReturnsNullopt) {
    EXPECT_FALSE(_cache.Read({
        .query = "{ __typename }"
    }).has_value());
}

TEST_F(InMemoryCacheTest, WriteAndRead) {
    GraphQLRequest req {
        .query = "{ __typename }"
    };
    GraphQLResponse res {
        .data = json {
            {"__typename", "Query"}
        }
    };
    _cache.Write(req, res);
    auto hit = _cache.Read(req);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->data, res.data);
}

TEST_F(InMemoryCacheTest, Evict) {
    GraphQLRequest req {
        .query = "{ __typename }"
    };
    _cache.Write(req, GraphQLResponse{.data = json::object()});
    _cache.Evict(req);
    EXPECT_FALSE(_cache.Read(req).has_value());
}

TEST_F(InMemoryCacheTest, VariablesDifferentiateKeys) {
    GraphQLRequest req1 {
        .query = "query($id: ID!) { user(id: $id) { name } }",
        .variables = {
            {"id", "1"}
        }
    };
    GraphQLRequest req2 {
        .query = "query($id: ID!) { user(id: $id) { name } }",
        .variables = {
            {"id", "2"}
        }
    };
    _cache.Write(req1, GraphQLResponse{
        .data = {
            {"user", {
                {"name", "Alice"}
            }}
        }
    });
    EXPECT_FALSE(_cache.Read(req2).has_value());
}

class NormalizedCacheTest : public ::testing::Test {
protected:
    InMemoryCache _cache;
};

TEST_F(NormalizedCacheTest, NormalizesNestedObjectWithId) {
    GraphQLRequest req {
        .query = R"(
            query {
                user {
                    __typename
                    id
                    name
                }
            }
        )"
    };

    _cache.Write(req, GraphQLResponse{
        .data = json {
            {"user", {
                {"__typename", "User"},
                {"id", "1"},
                {"name", "Alice"}
            }}
        }
    });

    auto store = _cache.Extract();
    EXPECT_TRUE(store.contains(R"(User:"1")"));
    EXPECT_EQ(store[R"(User:"1")"]["name"], "Alice");
    EXPECT_EQ(store[R"(User:"1")"]["__typename"], "User");
}

TEST_F(NormalizedCacheTest, ReadReconstructsFromNormalizedStore) {
    GraphQLRequest req {
        .query = R"(
            query {
                user {
                    __typename
                    id
                    name
                }
            }
        )"
    };

    _cache.Write(req, GraphQLResponse{
        .data = json {
            {"user", {
                {"__typename", "User"},
                {"id", "1"},
                {"name", "Alice"}
            }}
        }
    });

    auto hit = _cache.Read(req);
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ((*hit->data)["user"]["name"], "Alice");
    EXPECT_EQ((*hit->data)["user"]["id"], "1");
}

TEST_F(NormalizedCacheTest, EntityDeduplication) {
    _cache.Write({
        .query = R"(query { user { __typename id name } })"
    }, {
        .data = json {
            {"user", {
                {"__typename", "User"},
                {"id", "1"},
                {"name", "Alice"}
            }}
        }
    });

    _cache.Write({
        .query = R"(query { author { __typename id name email } })"
    }, {
        .data = json {
            {"author", {
                {"__typename", "User"},
                {"id", "1"},
                {"name", "Alice Updated"},
                {"email", "alice@test.com"}
            }}
        }
    });

    auto store = _cache.Extract();
    EXPECT_EQ(store[R"(User:"1")"]["name"], "Alice Updated");
    EXPECT_EQ(store[R"(User:"1")"]["email"], "alice@test.com");
}

TEST_F(NormalizedCacheTest, MutationUpdatesEntityAndAffectsSubsequentReads) {
    auto query = R"(query { user { __typename id name } })";
    auto mutation = R"(mutation { updateUser { __typename id name } })";

    _cache.Write({
        .query = query
    }, {
        .data = json{
            {"user", {
                {"__typename", "User"},
                {"id", "1"},
                {"name", "Alice"}
            }}
        }
    });

    _cache.Write({
        .query = mutation,
        .type = OperationType::Mutation
    }, {
        .data = json{
            {"updateUser", {
                {"__typename", "User"},
                {"id", "1"},
                {"name", "Bob"}
            }}
        }
    });

    auto hit = _cache.Read({.query = query});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ((*hit->data)["user"]["name"], "Bob");
}

TEST_F(NormalizedCacheTest, NormalizesArray) {
    auto query = R"(query { users { __typename id name } })";

    _cache.Write({
        .query = query
    }, {
        .data = json{
            {"users", json::array({
                {
                    {"__typename", "User"},
                    {"id", "1"},
                    {"name", "Alice"}
                },
                {
                    {"__typename", "User"},
                    {"id", "2"},
                    {"name", "Bob"}
                }
            })}
        }
    });

    auto store = _cache.Extract();
    EXPECT_TRUE(store.contains(R"(User:"1")"));
    EXPECT_TRUE(store.contains(R"(User:"2")"));

    auto hit = _cache.Read({.query = query});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ((*hit->data)["users"].size(), 2);
    EXPECT_EQ((*hit->data)["users"][0]["name"], "Alice");
    EXPECT_EQ((*hit->data)["users"][1]["name"], "Bob");
}

TEST_F(NormalizedCacheTest, HandlesNestedEntities) {
    auto query = R"(
        query {
            post {
                __typename
                id
                title
                author {
                    __typename
                    id
                    name
                }
            }
        }
    )";

    _cache.Write({
        .query = query
    }, {
        .data = json{
            {"post", {
                {"__typename", "Post"},
                {"id", "10"},
                {"title", "Hello"},
                {"author", {
                    {"__typename", "User"},
                    {"id", "1"},
                    {"name", "Alice"}
                }}
            }}
        }
    });

    auto store = _cache.Extract();
    EXPECT_TRUE(store.contains(R"(Post:"10")"));
    EXPECT_TRUE(store.contains(R"(User:"1")"));
    EXPECT_TRUE(store[R"(Post:"10")"]["author"].contains("__ref"));

    auto hit = _cache.Read({.query = query});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ((*hit->data)["post"]["title"], "Hello");
    EXPECT_EQ((*hit->data)["post"]["author"]["name"], "Alice");
}

TEST_F(NormalizedCacheTest, HandlesNullValues) {
    auto query = R"(query { user { __typename id name email } })";

    _cache.Write({
        .query = query
    }, {
        .data = json{
            {"user", {
                {"__typename", "User"},
                {"id", "1"},
                {"name", "Alice"},
                {"email", nullptr}
            }}
        }
    });

    auto hit = _cache.Read({.query = query});
    ASSERT_TRUE(hit.has_value());
    EXPECT_TRUE((*hit->data)["user"]["email"].is_null());
}

TEST_F(NormalizedCacheTest, EvictEntityRemovesFromStore) {
    auto query = R"(query { user { __typename id name } })";

    _cache.Write({
        .query = query
    }, {
        .data = json{
            {"user", {
                {"__typename", "User"},
                {"id", "1"},
                {"name", "Alice"}
            }}
        }
    });

    _cache.EvictEntity(R"(User:"1")");

    auto store = _cache.Extract();
    EXPECT_FALSE(store.contains(R"(User:"1")"));
}

TEST_F(NormalizedCacheTest, ScalarFieldsWithoutTypename) {
    auto query = R"(query { hello })";

    _cache.Write({
        .query = query
    }, {
        .data = json{
            {"hello", "world"}
        }
    });

    auto hit = _cache.Read({.query = query});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ((*hit->data)["hello"], "world");
}

TEST_F(NormalizedCacheTest, AliasedFields) {
    auto query = R"(query {
        first: user(id: "1") { __typename id name }
        second: user(id: "2") { __typename id name }
    })";

    _cache.Write({
        .query = query
    }, {
        .data = json{
            {"first", {
                {"__typename", "User"},
                {"id", "1"},
                {"name", "Alice"}
            }},
            {"second", {
                {"__typename", "User"},
                {"id", "2"},
                {"name", "Bob"}
            }}
        }
    });

    auto hit = _cache.Read({.query = query});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ((*hit->data)["first"]["name"], "Alice");
    EXPECT_EQ((*hit->data)["second"]["name"], "Bob");
}

TEST(TypePolicyTest, CustomKeyFields) {
    InMemoryCache cache(InMemoryCacheOptions{
        .typePolicies = {
            {"Book", TypePolicy{
                .keyFields = {"isbn"}
            }}
        }
    });

    cache.Write({
        .query = R"(query { book { __typename isbn title } })"
    }, {
        .data = json{
            {"book", {
                {"__typename", "Book"},
                {"isbn", "978-0"},
                {"title", "GraphQL in Action"}
            }}
        }
    });

    auto store = cache.Extract();
    EXPECT_TRUE(store.contains(R"(Book:"978-0")"));
    EXPECT_EQ(store[R"(Book:"978-0")"]["title"], "GraphQL in Action");
}

TEST(TypePolicyTest, CompositeKeyFields) {
    InMemoryCache cache(InMemoryCacheOptions{
        .typePolicies = {
            {"Review", TypePolicy{
                .keyFields = {"bookId", "authorId"}
            }}
        }
    });

    cache.Write({
        .query = R"(query { review { __typename bookId authorId rating } })"
    }, {
        .data = json{
            {"review", {
                {"__typename", "Review"},
                {"bookId", "b1"},
                {"authorId", "a1"},
                {"rating", 5}
            }}
        }
    });

    auto store = cache.Extract();
    EXPECT_TRUE(store.contains(R"(Review:"b1"|"a1")"));
}

TEST(TypePolicyTest, ExtractReturnsFullStore) {
    InMemoryCache cache;

    cache.Write({
        .query = R"(query { user { __typename id name } })"
    }, {
        .data = json{
            {"user", {
                {"__typename", "User"},
                {"id", "1"},
                {"name", "Alice"}
            }}
        }
    });

    auto store = cache.Extract();
    EXPECT_FALSE(store.empty());
    EXPECT_TRUE(store.contains(R"(User:"1")"));
}
