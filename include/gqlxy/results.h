#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace gqlxy {

struct GraphQLError {
    std::string message;
    std::vector<std::string> path;
};

using GraphQLErrors = std::vector<GraphQLError>;

struct GraphQLResult {
    std::optional<nlohmann::json> data;
    std::optional<GraphQLErrors> errors;
};

struct GraphQLRequest {
    std::string query;
    nlohmann::json variables = nullptr;
    std::optional<std::string> operationName;
};

} // namespace gqlxy
