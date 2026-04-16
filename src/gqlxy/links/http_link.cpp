#include <gqlxy/links/http_link.h>

#include <boost/beast/http.hpp>
#include <gqlxy/internal/asio_context.h>
#include <gqlxy/internal/http/http_stream.h>
#include <gqlxy/internal/http/https_stream.h>
#include <gqlxy/internal/http/serialization.h>
#include <gqlxy/internal/url.h>
#include <map>
#include <string>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace boost::asio;
namespace http = boost::beast::http;
using namespace rxcpp;

HttpLink::HttpLink(const HttpLinkOptions& options) : _options(options) {}

shared_ptr<IHttpStream> CreateStream(const any_io_executor& executor, const Url& url, const optional<string>& caCert) {
    return url.tls ? static_pointer_cast<IHttpStream>(make_shared<HttpsStream>(executor, url, caCert))
                   : make_shared<HttpStream>(executor, url);
}

Observable<GraphQLResponse> HttpLink::Execute(const GraphQLRequest& request) {
    return observable<>::create<GraphQLResponse>([opts = _options, request](const auto& sub) {
        try {
            const auto url = ParseHttpUrl(opts.url);
            auto stream = CreateStream(AsioContext::Get().get_executor(), url, opts.caCert);
            stream->Send(request, opts.headers)
                .subscribe(
                    [sub, stream](const GraphQLResponse& r) { sub.on_next(r); },
                    [sub, stream](const exception_ptr& e) { sub.on_error(e); },
                    [sub, stream]() { sub.on_completed(); });
        } catch (...) {
            sub.on_error(current_exception());
        }
    });
}
