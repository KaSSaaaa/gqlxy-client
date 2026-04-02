#include <gqlxy/links/sse_link.h>

namespace gqlxy {

SseLink::SseLink(SseLinkOptions options) : options_(std::move(options)) {}

Observable<GraphQLResult> SseLink::Execute(const GraphQLRequest& request) {
    // TODO: implement using boost::beast HTTP with chunked/SSE streaming (graphql-sse protocol)
    return rxcpp::observable<>::create<GraphQLResult>([](rxcpp::subscriber<GraphQLResult> s) {
        s.on_error(std::make_exception_ptr(std::runtime_error("SseLink not yet implemented")));
    });
}

} // namespace gqlxy
