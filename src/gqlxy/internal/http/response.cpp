#include <gqlxy/internal/http/response.h>
#include <gqlxy/internal/utils/ranges.h>

#include <nlohmann/json.hpp>

using namespace std;
using namespace nlohmann;

namespace gqlxy::internal {

GraphQLResult parse_json_response(const string& body) {
    const auto parsedBody = json::parse(body);
    return GraphQLResult{
        .data = make_optional_if(parsedBody.contains("data") && !parsedBody["data"].is_null(), [&parsedBody]() {
            return parsedBody["data"];
        }),
        .errors = make_optional_if(parsedBody.contains("errors") && parsedBody["errors"].is_array(), [&parsedBody]() {
            return to_vector(parsedBody["errors"] | views::transform([](const json& error) {
                return GraphQLError{
                    .message = error.value("message", "Unknown error"),
                    .path = make_optional_if(error.contains("path") && error["path"].is_array(), [&]() {
                        return to_vector(error["path"] | views::transform([](const json& p) {
                            return p.is_string()? p.get<string>() : p.dump();
                        }));
                    }).value_or(vector<string>{}),
                };
            }));
        }),
    };
}

GraphQLResult map_http_error(int status, string_view reason) {
    return GraphQLResult{
        .errors = GraphQLErrors{{
            .message = format("HTTP {} {}", status, reason),
        }},
    };
}

// Parses a graphql-sse response body into individual GraphQLResult values.
// Each SSE event block has the form:
//   event: next
//   data: <json>
//
//   event: complete
//
vector<GraphQLResult> parse_sse_body(const string& body) {
    return flat_map(chunk_by_blank(to_vector(split(body, '\n') | views::transform([](string line) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            return line;
        }))),
        [](const vector<string>& block) -> vector<GraphQLResult> {
            auto event_line = find_optional(block, [](const auto& l) {
                return l.starts_with("event:");
            });
            if (!event_line || trim(event_line->substr(6)) != "next") return {};
            auto data_line = find_optional(block, [](const auto& l) {
                return l.starts_with("data:");
            });
            if (!data_line) return {};
            return { parse_json_response(trim(data_line->substr(5))) };
        });
}

}
