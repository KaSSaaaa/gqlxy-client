#pragma once

#include <gqlxy/observable.h>
#include <gqlxy/client/results.h>

namespace gqlxy {

using Headers = std::map<std::string, std::string>;

struct LinkOptions {
    std::string url;
    Headers headers;
    std::optional<std::string> caCert;
};

class Link {
public:
    virtual ~Link() = default;
    virtual Observable<GraphQLResponse> Execute(const GraphQLRequest& request) = 0;
};

}
