#pragma once

#include <gqlxy/link.h>
#include <gqlxy/client/results.h>
#include <rxcpp/rx.hpp>

namespace gqlxy::internal {

class IHttpStream {
public:
    virtual ~IHttpStream() = default;

    virtual rxcpp::observable<GraphQLResponse> Send(const GraphQLRequest& request, const Headers& headers) = 0;
};

}