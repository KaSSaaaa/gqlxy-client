#pragma once

#include <gqlxy/link.h>
#include <string>

namespace gqlxy {

struct HttpLinkOptions {
    std::string url;
};

class HttpLink : public Link {
public:
    explicit HttpLink(HttpLinkOptions options);

    Observable<GraphQLResult> Execute(const GraphQLRequest& request) override;

private:
    HttpLinkOptions options_;
};

} // namespace gqlxy
