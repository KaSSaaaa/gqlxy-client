#pragma once

#include "fetch_policy.h"
#include <gqlxy/core/parser/ast/operation_definition.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>

namespace gqlxy {

struct GraphQLRequest {
    std::string query;
    nlohmann::json variables = nullptr;
    std::optional<std::string> operationName;
    parser::OperationType type = parser::OperationType::QUERY;
    FetchPolicy policy = FetchPolicy::CacheFirst;
};

}
