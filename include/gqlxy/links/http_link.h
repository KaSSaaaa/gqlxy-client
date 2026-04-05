#pragma once

#include <gqlxy/link.h>
#include <map>
#include <string>

namespace gqlxy {

struct HttpLinkOptions {
    std::string url;
    std::map<std::string, std::string> headers;
};

class HttpLink : public Link {
public:
    HttpLink(const HttpLinkOptions& options);

    Observable<GraphQLResult> Execute(const GraphQLRequest& request) override;

private:
    HttpLinkOptions _options;
};

}
