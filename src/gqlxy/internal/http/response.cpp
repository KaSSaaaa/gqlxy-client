#include <gqlxy/internal/http/response.h>

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/status.hpp>
#include <gqlxy/internal/utils/ranges.h>
#include <sstream>

#include <nlohmann/json.hpp>

using namespace std;
using namespace nlohmann;
using namespace boost::beast::http;

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

GraphQLResult MapHttpError(status status, string_view reason) {
    stringstream statusStream;
    statusStream << "HTTP " << status << " " << reason;
    return GraphQLResult{
        .errors = GraphQLErrors{
            {.message = statusStream.str()}
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

static bool IsSseComplete(const vector<string>& block) {
    auto event = find_optional(block, [](const auto& l) { return l.starts_with("event:"); });
    return event && trim(event->substr(6)) == "complete";
}

static auto ToSseLines(const string& text) {
    return to_vector(split(text, '\n') | views::transform([](string line) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return line;
    }));
}

vector<GraphQLResult> ParseSseBody(const string& body) {
    return flat_map(chunk_by_blank(ToSseLines(body)), ParseSseBlock);
}

SseDrain DrainSseEvents(const string& pending) {
    size_t last_sep = string::npos;
    size_t pos = 0;
    while ((pos = pending.find("\n\n", pos)) != string::npos) {
        last_sep = pos;
        pos += 2;
    }
    if (last_sep == string::npos) return {{}, pending, false};

    const auto complete = pending.substr(0, last_sep + 2);
    const auto remaining = pending.substr(last_sep + 2);

    vector<GraphQLResult> results;
    bool completed = false;
    for (const auto& block : chunk_by_blank(ToSseLines(complete))) {
        if (IsSseComplete(block)) {
            completed = true;
            break;
        }
        auto r = ParseSseBlock(block);
        results.insert(results.end(), r.begin(), r.end());
    }
    return {std::move(results), remaining, completed};
}

}
