#pragma once

#include <gqlxy/link.h>
#include <gqlxy/links/http_link.h>

#include <boost/uuid/random_generator.hpp>
#include <memory>

namespace gqlxy {
namespace internal {
class WsConnectionContext;
}

using WsLinkOptions = LinkOptions;

class WsLink : public Link {
public:
    WsLink(const WsLinkOptions& options);
    ~WsLink() override;

    Observable<GraphQLResponse> Execute(const GraphQLRequest& request) override;

private:
    WsLinkOptions _options;
    std::shared_ptr<internal::WsConnectionContext> _connection;
    boost::uuids::random_generator _uuidGenerator;
};

}
