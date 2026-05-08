#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace gqlxy {

struct TypePolicy {
    std::vector<std::string> keyFields = {"id"};
};

struct InMemoryCacheOptions {
    std::unordered_map<std::string, TypePolicy> typePolicies;
};

}
