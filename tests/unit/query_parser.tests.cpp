#include <gqlxy/internal/query_parser.h>
#include <gtest/gtest.h>

using namespace gqlxy::internal;

TEST(QueryParserTest, ParsesSimpleQuery) {
    auto op = ParseQuery("{ hello }");
    EXPECT_EQ(op.type, ParsedOperationType::Query);
    ASSERT_EQ(op.selections.size(), 1);
    auto& field = std::get<SelectionField>(op.selections[0]);
    EXPECT_EQ(field.name, "hello");
}

TEST(QueryParserTest, ParsesExplicitQueryKeyword) {
    auto op = ParseQuery("query { user { name } }");
    EXPECT_EQ(op.type, ParsedOperationType::Query);
    ASSERT_EQ(op.selections.size(), 1);
    auto& field = std::get<SelectionField>(op.selections[0]);
    EXPECT_EQ(field.name, "user");
    ASSERT_EQ(field.selections.size(), 1);
    EXPECT_EQ(std::get<SelectionField>(field.selections[0]).name, "name");
}

TEST(QueryParserTest, ParsesMutation) {
    auto op = ParseQuery("mutation { createUser { id } }");
    EXPECT_EQ(op.type, ParsedOperationType::Mutation);
}

TEST(QueryParserTest, ParsesSubscription) {
    auto op = ParseQuery("subscription { onMessage { text } }");
    EXPECT_EQ(op.type, ParsedOperationType::Subscription);
}

TEST(QueryParserTest, ParsesNamedOperation) {
    auto op = ParseQuery("query GetUser { user { name } }");
    ASSERT_TRUE(op.name.has_value());
    EXPECT_EQ(*op.name, "GetUser");
}

TEST(QueryParserTest, ParsesFieldAlias) {
    auto op = ParseQuery("{ myUser: user { name } }");
    auto& field = std::get<SelectionField>(op.selections[0]);
    EXPECT_EQ(field.name, "user");
    ASSERT_TRUE(field.alias.has_value());
    EXPECT_EQ(*field.alias, "myUser");
}

TEST(QueryParserTest, ParsesArguments) {
    auto op = ParseQuery(R"({ user(id: "123", active: true) { name } })");
    auto& field = std::get<SelectionField>(op.selections[0]);
    EXPECT_EQ(field.arguments["id"], "123");
    EXPECT_EQ(field.arguments["active"], true);
}

TEST(QueryParserTest, ParsesVariableReferences) {
    auto op = ParseQuery("query($id: ID!) { user(id: $id) { name } }");
    auto& field = std::get<SelectionField>(op.selections[0]);
    EXPECT_TRUE(field.arguments["id"].is_object());
    EXPECT_EQ(field.arguments["id"]["$var"], "id");
}

TEST(QueryParserTest, ParsesInlineFragment) {
    auto op = ParseQuery(R"({
        node {
            ... on User { name }
            ... on Post { title }
        }
    })");
    auto& node = std::get<SelectionField>(op.selections[0]);
    ASSERT_EQ(node.selections.size(), 2);
    auto& frag1 = std::get<InlineFragment>(node.selections[0]);
    ASSERT_TRUE(frag1.typeCondition.has_value());
    EXPECT_EQ(*frag1.typeCondition, "User");
}

TEST(QueryParserTest, ParsesFragmentSpread) {
    auto op = ParseQuery(R"(
        query { user { ...UserFields } }
        fragment UserFields on User { name email }
    )");
    auto& user = std::get<SelectionField>(op.selections[0]);
    ASSERT_EQ(user.selections.size(), 1);
    auto& spread = std::get<FragmentSpread>(user.selections[0]);
    EXPECT_EQ(spread.name, "UserFields");
    ASSERT_EQ(op.fragments.size(), 1);
    EXPECT_EQ(op.fragments[0].name, "UserFields");
    EXPECT_EQ(op.fragments[0].typeCondition, "User");
}

TEST(QueryParserTest, ParsesNestedSelectionSets) {
    auto op = ParseQuery(R"({
        post {
            id
            author {
                name
                avatar { url }
            }
        }
    })");
    auto& post = std::get<SelectionField>(op.selections[0]);
    ASSERT_EQ(post.selections.size(), 2);
    auto& author = std::get<SelectionField>(post.selections[1]);
    EXPECT_EQ(author.name, "author");
    ASSERT_EQ(author.selections.size(), 2);
    auto& avatar = std::get<SelectionField>(author.selections[1]);
    EXPECT_EQ(avatar.name, "avatar");
}
