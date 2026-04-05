#pragma once

#include <gqlxy/client/results.h>
#include <nlohmann/json.hpp>

namespace gqlxy::internal {

inline nlohmann::json SerializeRequest(const GraphQLRequest& req) {
    nlohmann::json body {
        {"query", req.query}
    };
    if (!req.variables.is_null()) body["variables"] = req.variables;
    if (req.operationName) body["operationName"] = *req.operationName;
    return body;
}

}
