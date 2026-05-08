#include <gqlxy/client/internal/http/response.h>
#include <gqlxy/core/results.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace gqlxy::internal;
using namespace nlohmann;

static string NextEvent(const string& data) {
    return format(R"(event:next
data:{}

)", data);
}

static string CompleteEvent() {
    return R"(event:complete

)";
}

TEST(ParseSseEvents, EmptyStringReturnsEmpty) {
    auto [results, remaining, completed] = ParseSseEvents("");
    EXPECT_TRUE(results.empty());
    EXPECT_EQ(remaining, "");
    EXPECT_FALSE(completed);
}

TEST(ParseSseEvents, PartialEventReturnedAsRemaining) {
    auto partial = R"(event:next
data:{"data":null})";
    auto [results, remaining, completed] = ParseSseEvents(partial);
    EXPECT_TRUE(results.empty());
    EXPECT_EQ(remaining, partial);
    EXPECT_FALSE(completed);
}

TEST(ParseSseEvents, SingleNextEventEmitsResult) {
    string payload = R"({"data":{"__typename":"Query"}})";
    auto [results, remaining, completed] = ParseSseEvents(NextEvent(payload));
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].data, json::parse(payload)["data"]);
    EXPECT_TRUE(remaining.empty());
    EXPECT_FALSE(completed);
}

TEST(ParseSseEvents, SingleNextEventWithErrors) {
    string payload = R"({"errors":[{"message":"oops"}]})";
    auto [results, remaining, completed] = ParseSseEvents(NextEvent(payload));
    ASSERT_EQ(results.size(), 1u);
    ASSERT_TRUE(results[0].errors.has_value());
    EXPECT_EQ(results[0].errors->at(0).message, "oops");
    EXPECT_FALSE(completed);
}

TEST(ParseSseEvents, MultipleNextEventsEmitAllResults) {
    auto input = format("{}{}{}", NextEvent(R"({"data":{"n":1}})"), NextEvent(R"({"data":{"n":2}})"), NextEvent(R"({"data":{"n":3}})"));
    auto [results, remaining, completed] = ParseSseEvents(input);
    ASSERT_EQ(results.size(), 3u);
    EXPECT_EQ(results[0].data, json({{"n", 1}}));
    EXPECT_EQ(results[1].data, json({{"n", 2}}));
    EXPECT_EQ(results[2].data, json({{"n", 3}}));
    EXPECT_TRUE(remaining.empty());
    EXPECT_FALSE(completed);
}

TEST(ParseSseEvents, CompleteEventFollowedByPartialLeavesRemaining) {
    auto partial = R"(event:next
data:{"data":null})";
    auto input = format("{}{}", NextEvent(R"({"data":{"ok":true}})"), partial);
    auto [results, remaining, completed] = ParseSseEvents(input);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(remaining, partial);
    EXPECT_FALSE(completed);
}

TEST(ParseSseEvents, CompleteEventSetsCompleted) {
    auto [results, remaining, completed] = ParseSseEvents(CompleteEvent());
    EXPECT_TRUE(results.empty());
    EXPECT_TRUE(remaining.empty());
    EXPECT_TRUE(completed);
}

TEST(ParseSseEvents, ResultsBeforeCompleteAreEmitted) {
    auto input = format("{}{}", NextEvent(R"({"data":{"n":1}})"), CompleteEvent());
    auto [results, remaining, completed] = ParseSseEvents(input);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].data, json({{"n", 1}}));
    EXPECT_TRUE(completed);
}

TEST(ParseSseEvents, EventsAfterCompleteAreIgnored) {
    auto input = CompleteEvent() + NextEvent(R"({"data":{"n":1}})");
    auto [results, remaining, completed] = ParseSseEvents(input);
    EXPECT_TRUE(results.empty());
    EXPECT_TRUE(completed);
}

TEST(ParseSseEvents, UnknownEventTypeIsIgnored) {
    auto input = format(R"(event:connection_ack
data:{{}}

{})", NextEvent(R"({"data":{"ok":true}})"));
    auto [results, remaining, completed] = ParseSseEvents(input);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_FALSE(completed);
}

TEST(ParseSseEvents, CrlfLineEndingsAreHandled) {
    auto input = format("event:next\r\n{}\r\n\r\n", R"(data:{"data":{"n":42}})");
    auto [results, remaining, completed] = ParseSseEvents(input);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_EQ(results[0].data, json({{"n", 42}}));
    EXPECT_FALSE(completed);
}

TEST(ParseSseEvents, NextEventWithoutDataFieldIsIgnored) {
    auto input = R"(event:next

)";
    auto [results, remaining, completed] = ParseSseEvents(input);
    EXPECT_TRUE(results.empty());
    EXPECT_FALSE(completed);
}
