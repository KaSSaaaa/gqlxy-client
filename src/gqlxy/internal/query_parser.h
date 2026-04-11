#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace gqlxy::internal {

struct SelectionField;
struct FragmentSpread;
struct InlineFragment;

using Selection = std::variant<SelectionField, FragmentSpread, InlineFragment>;

struct SelectionField {
    std::string name;
    std::optional<std::string> alias;
    nlohmann::json arguments;
    std::vector<Selection> selections;
};

struct FragmentSpread {
    std::string name;
};

struct InlineFragment {
    std::optional<std::string> typeCondition;
    std::vector<Selection> selections;
};

struct FragmentDefinition {
    std::string name;
    std::string typeCondition;
    std::vector<Selection> selections;
};

enum class ParsedOperationType {
    Query,
    Mutation,
    Subscription
};

struct ParsedOperation {
    ParsedOperationType type = ParsedOperationType::Query;
    std::optional<std::string> name;
    std::vector<Selection> selections;
    std::vector<FragmentDefinition> fragments;
};

ParsedOperation ParseQuery(const std::string& query);

}
