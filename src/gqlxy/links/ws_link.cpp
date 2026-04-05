#include <gqlxy/links/ws_link.h>

namespace gqlxy {

WsLink::WsLink(WsLinkOptions options) : _options(std::move(options)) {}

Observable<GraphQLResult> WsLink::Execute(const GraphQLRequest& request) {
    // TODO: implement using boost::beast WebSocket + graphql-transport-ws protocol
    return rxcpp::observable<>::create<GraphQLResult>([](rxcpp::subscriber<GraphQLResult> s) {
        s.on_error(std::make_exception_ptr(std::runtime_error("WsLink not yet implemented")));
    });
}

}
