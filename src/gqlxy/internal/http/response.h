#pragma once

#include <boost/beast/http/status.hpp>
#include <gqlxy/client/results.h>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace gqlxy::internal {

//TODO simplify

GraphQLResponse ParseJsonResponse(const std::string& body);
GraphQLResponse ParseJsonPayload(const nlohmann::json& body);
GraphQLResponse MapHttpError(boost::beast::http::status status, std::string_view reason);
std::vector<GraphQLResponse> ParseSseBody(const std::string& body);

struct SseEvents {
    std::vector<GraphQLResponse> results;
    std::string remaining;
    bool completed;
};

// Extracts complete SSE events from pending, returning parsed results, leftover text,
// and whether an "event: complete" was seen.
SseDrain DrainSseEvents(const std::string& pending);

}
