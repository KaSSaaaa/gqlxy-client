#include "query_parser.h"

#include <graphqlservice/GraphQLParse.h>
#include <graphqlservice/internal/Grammar.h>
#include <stdexcept>

using namespace std;
using namespace gqlxy::internal;
using namespace graphql;

nlohmann::json ParseInputValue(const peg::ast_node& node);

nlohmann::json ParseObjectValue(const peg::ast_node& node) {
    auto obj = nlohmann::json::object();
    for (const auto& child : node.children) {
        if (!child || !child->is_type<peg::object_field>()) continue;
        string key;
        nlohmann::json val;
        for (const auto& fc : child->children) {
            if (fc->is_type<peg::object_field_name>()) key = fc->string();
            else if (fc->is_type<peg::input_value>()) val = ParseInputValue(*fc);
        }
        obj[key] = val;
    }
    return obj;
}

nlohmann::json ParseListValue(const peg::ast_node& node) {
    auto arr = nlohmann::json::array();
    for (const auto& child : node.children) {
        if (child && child->is_type<peg::input_value>())
            arr.push_back(ParseInputValue(*child));
    }
    return arr;
}

nlohmann::json ParseInputValue(const peg::ast_node& node) {
    if (node.is_type<peg::integer_value>()) return stoi(node.string());
    if (node.is_type<peg::float_value>()) return stod(node.string());
    if (node.is_type<peg::string_value>()) {
        if (!node.children.empty()) return string(node.children.front()->string_view());
        return string(node.string_view());
    }
    if (node.is_type<peg::true_keyword>()) return true;
    if (node.is_type<peg::false_keyword>()) return false;
    if (node.is_type<peg::null_keyword>()) return nullptr;
    if (node.is_type<peg::enum_value>()) return node.string();
    if (node.is_type<peg::list_value>()) return ParseListValue(node);
    if (node.is_type<peg::object_value>()) return ParseObjectValue(node);
    if (node.is_type<peg::variable_value>()) {
        auto varName = node.has_content() ? node.string() : node.children.front()->string();
        if (!varName.empty() && varName[0] == '$') varName = varName.substr(1);
        return nlohmann::json{{"$var", varName}};
    }
    if (node.is_type<peg::input_value>()) {
        for (const auto& child : node.children) {
            if (child) return ParseInputValue(*child);
        }
    }
    return nullptr;
}

nlohmann::json ParseArguments(const peg::ast_node& node) {
    auto args = nlohmann::json::object();
    for (const auto& child : node.children) {
        if (!child || !child->is_type<peg::arguments>()) continue;
        for (const auto& arg : child->children) {
            if (!arg || !arg->is_type<peg::argument>()) continue;
            string key;
            nlohmann::json val;
            for (const auto& ac : arg->children) {
                if (ac->is_type<peg::argument_name>()) key = ac->string();
                else val = ParseInputValue(*ac);
            }
            args[key] = val;
        }
    }
    return args;
}

vector<Selection> ParseSelections(const peg::ast_node& node);

SelectionField ParseField(const peg::ast_node& node) {
    SelectionField field;
    for (const auto& child : node.children) {
        if (!child) continue;
        if (child->is_type<peg::alias_name>()) field.alias = child->string();
        if (child->is_type<peg::field_name>()) field.name = child->string();
    }
    field.arguments = ParseArguments(node);
    for (const auto& child : node.children) {
        if (child && child->is_type<peg::selection_set>()) {
            field.selections = ParseSelections(*child);
            break;
        }
    }
    return field;
}

InlineFragment ParseInlineFragment(const peg::ast_node& node) {
    InlineFragment frag;
    for (const auto& child : node.children) {
        if (!child) continue;
        if (child->is_type<peg::type_condition>()) {
            for (const auto& tc : child->children) {
                if (tc && tc->is_type<peg::named_type>())
                    frag.typeCondition = tc->string();
            }
        }
        if (child->is_type<peg::selection_set>())
            frag.selections = ParseSelections(*child);
    }
    return frag;
}

vector<Selection> ParseSelections(const peg::ast_node& node) {
    vector<Selection> result;
    for (const auto& child : node.children) {
        if (!child) continue;
        if (child->is_type<peg::field>())
            result.emplace_back(ParseField(*child));
        else if (child->is_type<peg::fragment_spread>()) {
            for (const auto& fc : child->children) {
                if (fc && fc->is_type<peg::fragment_name>()) {
                    result.emplace_back(FragmentSpread {.name = fc->string()});
                    break;
                }
            }
        } else if (child->is_type<peg::inline_fragment>())
            result.emplace_back(ParseInlineFragment(*child));
    }
    return result;
}

ParsedOperationType ToOperationType(string_view type) {
    if (type == "mutation") return ParsedOperationType::Mutation;
    if (type == "subscription") return ParsedOperationType::Subscription;
    return ParsedOperationType::Query;
}

FragmentDefinition ParseFragmentDef(const peg::ast_node& node) {
    FragmentDefinition frag;
    for (const auto& child : node.children) {
        if (!child) continue;
        if (child->is_type<peg::fragment_name>()) frag.name = child->string();
        if (child->is_type<peg::type_condition>()) {
            for (const auto& tc : child->children) {
                if (tc && tc->is_type<peg::named_type>())
                    frag.typeCondition = tc->string();
            }
        }
        if (child->is_type<peg::selection_set>())
            frag.selections = ParseSelections(*child);
    }
    return frag;
}

namespace gqlxy::internal {

ParsedOperation ParseQuery(const string& query) {
    auto ast = peg::parseString(query);
    if (!ast.root) throw runtime_error("Failed to parse GraphQL query");

    ParsedOperation op;

    for (const auto& def : ast.root->children) {
        if (!def) continue;

        if (def->is_type<peg::operation_definition>()) {
            for (const auto& child : def->children) {
                if (!child) continue;
                if (child->is_type<peg::operation_type>())
                    op.type = ToOperationType(child->string());
                if (child->is_type<peg::operation_name>())
                    op.name = child->string();
                if (child->is_type<peg::selection_set>())
                    op.selections = ParseSelections(*child);
            }
        } else if (def->is_type<peg::fragment_definition>()) {
            op.fragments.push_back(ParseFragmentDef(*def));
        }
    }

    return op;
}

}
