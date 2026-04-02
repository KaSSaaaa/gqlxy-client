#pragma once

#include <gqlxy/observable.h>
#include <gqlxy/results.h>

namespace gqlxy {

class Link {
public:
    virtual ~Link() = default;
    virtual Observable<GraphQLResult> Execute(const GraphQLRequest& request) = 0;
};

} // namespace gqlxy
