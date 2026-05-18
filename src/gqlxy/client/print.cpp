#include <gqlxy/client/print.h>

#include <format>
#include <gqlxy/core/parser/ast/argument.h>
#include <gqlxy/core/parser/ast/directive.h>
#include <gqlxy/core/parser/ast/document.h>
#include <gqlxy/core/parser/ast/fragment_definition.h>
#include <gqlxy/core/parser/ast/operation_definition.h>
#include <gqlxy/core/parser/ast/selection.h>
#include <gqlxy/core/parser/ast/selection_set.h>
#include <gqlxy/core/parser/ast/variable_definition.h>
#include <gqlxy/core/utils/ranges.h>
#include <gqlxy/core/utils/visit.h>
#include <ranges>
#include <string>

using namespace std;
using namespace gqlxy::parser;
using namespace gqlxy::utils;

namespace gqlxy {

string PrintSelectionSet(const SelectionSet& selectionSet);
string PrintSelection(const Selection& sel);

string PrintArgument(const Argument& arg) {
    return format("{}: {}", arg.name, arg.value);
}

string PrintArguments(const vector<Argument>& args) {
    return !args.empty() ? format("({})", args | views::transform(PrintArgument) | join_with(", ")) : "";
}

string PrintDirective(const Directive& dir) {
    return format("@{}{}", dir.name, PrintArguments(dir.args));
}

string PrintDirectives(const vector<Directive>& dirs) {
    return !dirs.empty() ? format(" {}", dirs | views::transform(PrintDirective) | join_with(" ")) : "";
}

string PrintField(const Field& field) {
    auto prefix = field.alias.has_value() ? format("{}: ", field.alias.value()) : "";
    auto selectionSet = field.selectionSet.has_value() ? format(" {}", PrintSelectionSet(*field.selectionSet)) : "";
    return format("{}{}{}{}", prefix, field.name, PrintArguments(field.arguments), PrintDirectives(field.directives)) + selectionSet;
}

string PrintFragmentSpread(const FragmentSpread& spread) {
    return format("...{}{}", spread.name, PrintDirectives(spread.directives));
}

string PrintInlineFragment(const InlineFragment& frag) {
    auto typeCondition = frag.typeCondition.has_value() ? format(" on {}", frag.typeCondition.value()) : "";
    return format("...{}{} {}", typeCondition, PrintDirectives(frag.directives), PrintSelectionSet(frag.selectionSet.value()));
}

string PrintSelection(const Selection& sel) {
    return visit(
        overloaded {
            [&](const Field& field) { return PrintField(field); },
            [&](const FragmentSpread& spread) { return PrintFragmentSpread(spread); },
            [&](const InlineFragment& frag) { return PrintInlineFragment(frag); }},
        sel);
}

string PrintSelectionSet(const SelectionSet& selectionSet) {
    return format("{{ {} }}", selectionSet.selections | views::transform(PrintSelection) | join_with(" "));
}

string PrintVariableDefinitions(const vector<VariableDefinition>& vars) {
    return !vars.empty() ? format("({})", vars | views::transform([](const auto& var) {
        return format("${}: {}{}",
            var.name,
            var.type.ToString(),
            var.defaultValue.has_value() ? format(" = {}", var.defaultValue.value()) : "");
    }) | join_with(", ")) : "";
}

string PrintOperationDefinition(const OperationDefinition& op) {
    if (op.type._value == OperationType::QUERY && !op.name.has_value() && op.variableDefinitions.empty())
        return PrintSelectionSet(op.selectionSet);

    return format("{}{}{} {}",
        to_string(string(op.type._to_string()) | views::transform([](const auto& c) { return tolower(c); })),
        op.name.has_value() ? format(" {}", op.name.value()) : "",
        PrintVariableDefinitions(op.variableDefinitions),
        PrintSelectionSet(op.selectionSet));
}

string PrintFragmentDefinition(const FragmentDefinition& frag) {
    return format("fragment {} on {} {}", frag.name, frag.typeCondition, PrintSelectionSet(frag.selectionSet));
}

string Print(const Document& document) {
    return format("{}{}",
        document.operations
            | views::transform(PrintOperationDefinition)
            | join_with(" "),
        !document.fragments.empty()
            ? format(" {}", document.fragments
                | views::values
                | views::transform(PrintFragmentDefinition)
                | join_with(" ")) : "");
}

}
