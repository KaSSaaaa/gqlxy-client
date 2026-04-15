#pragma once

#include <gqlxy/observable.h>
#include <gqlxy/client/results.h>

namespace gqlxy {

struct LinkOptions {
    std::string url;
    std::map<std::string, std::string> headers;
    std::optional<std::string> caCert;
};

class Link {
public:
    virtual ~Link() = default;
    virtual Observable<GraphQLResult> Execute(const GraphQLRequest& request) = 0;
};

}
