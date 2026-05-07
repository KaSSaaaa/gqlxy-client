#include <gqlxy/parser/peg/parser/query/parse_document.h>
#include <gqlxy/print.h>
#include <gqlxy/transforms/add_typename.h>
#include <gqlxy/utils/ranges.h>
#include <gtest/gtest.h>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::parser;
using namespace gqlxy::utils;

inline size_t count_occurrences(std::string_view haystack, std::string_view needle) {
    if (needle.empty()) return 0;
    size_t count = 0;
    for (auto pos = haystack.find(needle); pos != std::string_view::npos; pos = haystack.find(needle, pos + needle.size()))
        ++count;
    return count;
}

static string Transform(const string& query) {
    return Print(AddTypename(ParseDocument(query)));
}

TEST(AddTypenameTransform, InjectsTypenameIntoSimpleQuery) {
    EXPECT_EQ(count_occurrences(Transform("{ user { id name } }"), "__typename"), 2u);
}

TEST(AddTypenameTransform, DoesNotDuplicateExistingTypename) {
    EXPECT_EQ(count_occurrences(Transform("{ user { __typename id name } }"), "__typename"), 2u);
}

TEST(AddTypenameTransform, InjectsIntoNestedFields) {
    EXPECT_EQ(count_occurrences(Transform("{ post { id author { name } } }"), "__typename"), 3u);
}

TEST(AddTypenameTransform, AddsTypenameToScalarOnlySelectionSet) {
    EXPECT_GE(count_occurrences(Transform("{ hello }"), "__typename"), 1u);
}

TEST(AddTypenameTransform, InjectsTypenameIntoFragmentDefinition) {
    EXPECT_EQ(count_occurrences(Transform("fragment UserFields on User { id name }"), "__typename"), 1u);
}

TEST(AddTypenameTransform, DoesNotDuplicateTypenameInFragment) {
    EXPECT_EQ(count_occurrences(Transform("fragment UserFields on User { __typename id name }"), "__typename"), 1u);
}

TEST(AddTypenameTransform, InjectsIntoInlineFragment) {
    EXPECT_GE(count_occurrences(Transform("{ node { ... on User { id name } } }"), "__typename"), 1u);
}

TEST(AddTypenameTransform, InjectsIntoMutation) {
    EXPECT_GE(count_occurrences(Transform("mutation { createUser { id name } }"), "__typename"), 1u);
}

TEST(AddTypenameTransform, InjectsIntoSubscription) {
    EXPECT_GE(count_occurrences(Transform("subscription { onMessage { id body } }"), "__typename"), 1u);
}

TEST(AddTypenameTransform, PreservesOperationName) {
    auto printed = Transform("query GetUser { user { id } }");
    EXPECT_NE(printed.find("GetUser"), string::npos);
    EXPECT_GE(count_occurrences(printed, "__typename"), 1u);
}

TEST(AddTypenameTransform, PreservesVariableDefinitions) {
    auto printed = Transform("query($id: ID!) { user(id: $id) { name } }");
    EXPECT_NE(printed.find("$id"), string::npos);
    EXPECT_GE(count_occurrences(printed, "__typename"), 1u);
}

TEST(AddTypenameTransform, PreservesFieldAlias) {
    auto printed = Transform("{ me: user { id } }");
    EXPECT_NE(printed.find("me: user"), string::npos);
    EXPECT_GE(count_occurrences(printed, "__typename"), 1u);
}

TEST(AddTypenameTransform, IdempotentWhenTypenameAlreadyPresent) {
    auto once = Transform("{ user { __typename id name } }");
    EXPECT_EQ(Print(AddTypename(ParseDocument(once))), once);
}
