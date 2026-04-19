#include <gqlxy/internal/http/response.h>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <gqlxy/utils/ranges.h>
#include <gqlxy/utils/optional.h>
#include <nlohmann/json.hpp>

using namespace std;
using namespace gqlxy::utils;
using namespace nlohmann;
using namespace boost::beast::http;

namespace gqlxy::internal {

GraphQLResponse ParseJsonPayload(const string& body) {
    return ParseJsonPayload(json::parse(body));
}

GraphQLResponse ParseJsonPayload(const json& body) {
    return GraphQLResponse{
        .data = make_optional_if(body.contains("data") && !body["data"].is_null(), [&body]() {
            return body["data"];
        }),
        .errors = make_optional_if(body.contains("errors") && body["errors"].is_array(), [&body]() {
            return to_vector(
                ranges::subrange(body["errors"].begin(), body["errors"].end())
                    | views::transform([](const auto& error) {
                        return GraphQLError {
                            .message = error.value("message", "Unknown error"),
                            .path = make_optional_if(error.contains("path") && error["path"].is_array(), [&]() {
                                return to_vector(
                                    ranges::subrange(error["path"].begin(), error["path"].end())
                                        | views::transform([](const json& p) {
                                            return p.is_string() ? p.get<string>() : p.dump();
                                        })
                                );
                            }).value_or(vector<string>{})
                        };
                    })
            );
        }),
    };
}

GraphQLResponse ConvertHttpError(status status, string_view reason) {
    return GraphQLResponse{
        .errors = GraphQLErrors{
            {.message = format("HTTP {} {}", to_string(status), reason)}
        },
    };
}

static vector<GraphQLResponse> ParseSseBlock(const vector<string>& block) {
    if (and_then(find_optional(block, [](const auto& l) { return l.starts_with("event:"); }), [](const auto& event) {
        return trim(event.substr(6));
    }) != "next")
        return {};

    return and_then(find_optional(block, [](const auto& l) { return l.starts_with("data:"); }), [](const auto& data) {
        return make_optional(vector{ParseJsonPayload(json::parse(string(trim(data.substr(5)))))});
    }).value_or(vector<GraphQLResponse>{});
}

SseEvents ParseSseEvents(const string& payload) {
    auto normalized = payload;
    erase(normalized, '\r');
    auto last = ranges::find_end(normalized, "\n\n"sv);
    if (last.empty()) return {{}, normalized, false};

    auto chunks = chunk_by_blank(split(string(normalized.begin(), last.end()), '\n'));
    auto complete = ranges::find_if(chunks, [](const auto& block) {
        return and_then(find_optional(block, [](const auto& l) { return l.starts_with("event:"); }), [](const auto& event) {
            return event.substr(6) == "complete";
        });
    });

    return {
        flat_map(ranges::subrange(chunks.begin(), complete), ParseSseBlock),
        string(last.end(), normalized.end()),
        complete != chunks.end()
    };
}

}
