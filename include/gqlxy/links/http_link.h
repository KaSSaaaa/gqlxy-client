#pragma once

#include <gqlxy/link.h>

namespace gqlxy {

using HttpLinkOptions = LinkOptions;

class HttpLink : public Link {
public:
    HttpLink(const HttpLinkOptions& options);

    Observable<GraphQLResult> Execute(const GraphQLRequest& request) override;

private:
    HttpLinkOptions _options;
};

}
