#pragma once

#include <gqlxy/link.h>

#include <boost/uuid/random_generator.hpp>
#include <map>
#include <memory>
#include <string>

namespace gqlxy::internal {
    class WsConnection;
}

namespace gqlxy {

struct WsLinkOptions {
    std::string url;
    std::map<std::string, std::string> headers;
};

class WsLink : public Link {
public:
    WsLink(const WsLinkOptions& options);
    ~WsLink() override;

    Observable<GraphQLResult> Execute(const GraphQLRequest& request) override;

private:
    WsLinkOptions _options;
    std::shared_ptr<internal::WsConnection> _connection;
    boost::uuids::random_generator _uuidGenerator;
};

}
