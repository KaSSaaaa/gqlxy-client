#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace gqlxy::internal {

struct ParsedSelection;

struct SelectionField {
    std::string name;
    std::optional<std::string> alias;
    nlohmann::json arguments;
    std::vector<ParsedSelection> selections;
};

struct FragmentSpread {
    std::string name;
};

struct InlineFragment {
    std::optional<std::string> typeCondition;
    std::vector<ParsedSelection> selections;
};

struct FragmentDefinition {
    std::string name;
    std::string typeCondition;
    std::vector<ParsedSelection> selections;
};

enum class ParsedOperationType {
    Query,
    Mutation,
    Subscription
};

struct ParsedOperation {
    ParsedOperationType type = ParsedOperationType::Query;
    std::optional<std::string> name;
    std::vector<ParsedSelection> selections;
    std::vector<FragmentDefinition> fragments;
};

struct ParsedSelection : std::variant<SelectionField, FragmentSpread, InlineFragment>{};

ParsedOperation ParseQuery(const std::string& query);

}
