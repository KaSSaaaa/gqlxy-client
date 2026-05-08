#pragma once

#include <gqlxy/client/link.h>
#include <gqlxy/client/client/results.h>
#include <rpp/observables/dynamic_observable.hpp>

namespace gqlxy::internal {

class IHttpStream {
public:
    virtual ~IHttpStream() = default;

    virtual rpp::dynamic_observable<GraphQLResponse> Send(const GraphQLRequest& request, const Headers& headers) = 0;
};

}
