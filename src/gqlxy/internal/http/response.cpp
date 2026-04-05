#include <gqlxy/internal/http/response.h>
#include <gqlxy/internal/utils/ranges.h>

#include <nlohmann/json.hpp>

using namespace std;
using namespace nlohmann;

namespace gqlxy::internal {

static vector<string> ParseErrorPath(const json& e) {
    if (!e.contains("path") || !e["path"].is_array()) return {};
    return to_vector(
        ranges::subrange(e["path"].begin(), e["path"].end()) | views::transform([](const json& p) {
            return p.is_string() ? p.get<string>() : p.dump();
        })
    );
}

static GraphQLError ParseError(const json& e) {
    return {
        .message = e.value("message", "Unknown error"),
        .path = ParseErrorPath(e)
    };
}

GraphQLResult ParseJsonPayload(const json& body) {
    return GraphQLResult{
        .data = make_optional_if(body.contains("data") && !body["data"].is_null(), [&body]() {
            return body["data"];
        }),
        .errors = make_optional_if(body.contains("errors") && body["errors"].is_array(), [&body]() {
            return to_vector(
                ranges::subrange(body["errors"].begin(), body["errors"].end()) | views::transform(ParseError)
            );
        }),
    };
}

GraphQLResult ParseJsonResponse(const string& body) {
    return ParseJsonPayload(json::parse(body));
}

GraphQLResult MapHttpError(int status, string_view reason) {
    return GraphQLResult{
        .errors = GraphQLErrors{
            {.message = format("HTTP {} {}", status, reason)}
        },
    };
}

static vector<GraphQLResult> ParseSseBlock(const vector<string>& block) {
    auto event = find_optional(block, [](const auto& l) { return l.starts_with("event:"); });
    if (!event || trim(event->substr(6)) != "next") return {};
    auto data = find_optional(block, [](const auto& l) { return l.starts_with("data:"); });
    if (!data) return {};
    return {ParseJsonResponse(trim(data->substr(5)))};
}

vector<GraphQLResult> ParseSseBody(const string& body) {
    return flat_map(chunk_by_blank(to_vector(split(body, '\n') | views::transform([](string line) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return line;
    }))), ParseSseBlock);
}

}
