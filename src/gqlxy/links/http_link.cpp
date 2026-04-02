#include <gqlxy/links/http_link.h>

namespace gqlxy {

HttpLink::HttpLink(HttpLinkOptions options) : options_(std::move(options)) {}

Observable<GraphQLResult> HttpLink::Execute(const GraphQLRequest& request) {
    // TODO: implement using boost::beast async HTTP POST
    return rxcpp::observable<>::create<GraphQLResult>([](rxcpp::subscriber<GraphQLResult> s) {
        s.on_error(std::make_exception_ptr(std::runtime_error("HttpLink not yet implemented")));
    });
}

} // namespace gqlxy
