#include <gqlxy/core/parser/peg/parser/query/parse_document.h>
#include <gqlxy/client/print.h>
#include <gtest/gtest.h>

using namespace std;
using namespace testing;
using namespace gqlxy;
using namespace gqlxy::parser;

class PrintTests : public TestWithParam<string> {
public:
};

static std::string RoundTrip(const std::string& query) {
    return Print(ParseDocument(query));
}

INSTANTIATE_TEST_SUITE_P(Default, PrintTests, Values(
    "{ __typename }",
    "query($id: ID!) { user(id: $id) { name } }",
    R"(query($id: ID! = "some-id") { user(id: $id) { name } })",
    "query GetUser { user { name } }",
    "mutation { createUser { id } }",
    "subscription { onMessage { text } }",
    "{ me: user { id } }",
    "{ user { address { city } } }",
    R"({ user(id: "1") { name } })",
    R"({ users(first: 10, after: "cursor") { id } })",
    "{ user { name @include(if: true) } }",
    "{ user { ...UserFields } } fragment UserFields on User { id name }",
    "{ node { ... on User { name } } }",
    "{ node { ... on User @defer { name } } }",
    "query A { __typename } mutation B { createUser { id } }",
    ""
));

TEST_P(PrintTests, SameOutput) {
    auto param = GetParam();
    EXPECT_EQ(RoundTrip(param), param);
}

TEST_P(PrintTests, RoundTripIsStable) {
    auto param = GetParam();
    auto once = RoundTrip(param);
    EXPECT_EQ(Print(ParseDocument(once)), once);
}
