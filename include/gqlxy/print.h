#pragma once

#include <gqlxy/parser/ast/document.h>

#include <string>

namespace gqlxy {

std::string Print(const parser::Document& document);

}
