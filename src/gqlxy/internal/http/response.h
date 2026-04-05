#pragma once

#include <gqlxy/client/results.h>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace gqlxy::internal {

GraphQLResult ParseJsonResponse(const std::string& body);
GraphQLResult ParseJsonPayload(const nlohmann::json& body);
GraphQLResult MapHttpError(int status, std::string_view reason);
std::vector<GraphQLResult> ParseSseBody(const std::string& body);

}
