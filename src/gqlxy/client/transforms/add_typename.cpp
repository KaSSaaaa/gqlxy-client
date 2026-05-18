#include <gqlxy/client/transforms/add_typename.h>

#include <algorithm>
#include <gqlxy/core/parser/ast/document.h>
#include <gqlxy/core/parser/ast/fragment_definition.h>
#include <gqlxy/core/parser/ast/operation_definition.h>
#include <gqlxy/core/parser/ast/selection.h>
#include <gqlxy/core/parser/ast/selection_set.h>
#include <gqlxy/core/utils/optional.h>
#include <gqlxy/core/utils/ranges.h>
#include <gqlxy/core/utils/visit.h>
#include <string>
#include <variant>

using namespace std;
using namespace gqlxy::parser;
using namespace gqlxy::utils;

namespace gqlxy {

SelectionSet TransformSelectionSet(const SelectionSet& selectionSet);

bool HasTypename(const SelectionSet& ss) {
    return ranges::any_of(ss.selections, [](const auto& sel) {
        auto field = get_if<Field>(&sel);
        return field != nullptr && field->name == "__typename" && !field->alias.has_value();
    });
}

bool HasAnyField(const SelectionSet& ss) {
    return ranges::any_of(ss.selections, [](const auto& sel) { return holds_alternative<Field>(sel); });
}

Selection TransformSelection(const Selection& sel) {
    return std::visit(
        overloaded {
            [](const Field& field) {
                return Selection {Field {
                    .alias = field.alias,
                    .name = field.name,
                    .arguments = field.arguments,
                    .directives = field.directives,
                    .selectionSet = and_then(
                        field.selectionSet,
                        [](const auto& selectionSet) {
                            return std::make_optional(TransformSelectionSet(selectionSet));
                        }),
                }};
            },
            [](const FragmentSpread& spread) { return Selection {spread}; },
            [](const InlineFragment& frag) {
                return Selection {InlineFragment {
                    .typeCondition = frag.typeCondition,
                    .directives = frag.directives,
                    .selectionSet = and_then(
                        frag.selectionSet,
                        [](const auto& selectionSet) {
                            return std::make_optional(TransformSelectionSet(selectionSet));
                        }),
                }};
            }},
        sel);
}

SelectionSet TransformSelectionSet(const SelectionSet& selectionSet) {
    return SelectionSet {
        .selections = concat(
            to_vector(selectionSet.selections | views::transform(TransformSelection)),
            HasAnyField(selectionSet) && !HasTypename(selectionSet)
                ? vector<Selection>{{ Field {.name = "__typename"} }}
                : vector<Selection>{}
        )
    };
}

Document AddTypename(const Document& document) {
    return Document {
        .operations = to_vector(document.operations | views::transform([](const auto& op) {
            return OperationDefinition {
                .type = op.type,
                .name = op.name,
                .variableDefinitions = op.variableDefinitions,
                .selectionSet = TransformSelectionSet(op.selectionSet),
            };
        })),
        .fragments = to_unordered_map(document.fragments | views::transform([](const auto& kvp) {
            auto [name, frag] = kvp;
            return make_pair(name, FragmentDefinition {
                .name = frag.name,
                .typeCondition = frag.typeCondition,
                .selectionSet = TransformSelectionSet(frag.selectionSet),
            });
        }))
    };
}

}
