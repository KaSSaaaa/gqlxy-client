#pragma once

#include <boost/beast/http/status.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <vector>

namespace gqlxy {
struct GraphQLResponse;
}

namespace gqlxy::internal {

struct SseEvents {
    std::vector<GraphQLResponse> results;
    std::string remaining;
    bool completed;
};

SseEvents ParseSseEvents(const std::string& payload);
GraphQLResponse ParseJsonPayload(const std::string& body);
GraphQLResponse ParseJsonPayload(const nlohmann::json& body);
GraphQLResponse ConvertHttpError(boost::beast::http::status status, std::string_view reason);


}
