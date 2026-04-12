#pragma once

#include <boost/beast/http/status.hpp>
#include <gqlxy/client/results.h>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace gqlxy::internal {

GraphQLResult ParseJsonResponse(const std::string& body);
GraphQLResult ParseJsonPayload(const nlohmann::json& body);
GraphQLResult MapHttpError(boost::beast::http::status status, std::string_view reason);
std::vector<GraphQLResult> ParseSseBody(const std::string& body);

struct SseDrain {
    std::vector<GraphQLResult> results;
    std::string remaining;
    bool completed;
};

// Extracts complete SSE events from pending, returning parsed results, leftover text,
// and whether an "event: complete" was seen.
SseDrain DrainSseEvents(const std::string& pending);

}
