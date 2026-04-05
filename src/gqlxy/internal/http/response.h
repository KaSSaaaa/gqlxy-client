#pragma once

#include <gqlxy/client/results.h>
#include <string>
#include <string_view>
#include <vector>

namespace gqlxy::internal {

GraphQLResult parse_json_response(const std::string& body);
GraphQLResult map_http_error(int status, std::string_view reason);
std::vector<GraphQLResult> parse_sse_body(const std::string& body);

}
