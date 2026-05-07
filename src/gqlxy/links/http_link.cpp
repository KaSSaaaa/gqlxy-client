#include <gqlxy/links/http_link.h>

#include <boost/beast/http.hpp>
#include <gqlxy/internal/asio_context.h>
#include <gqlxy/internal/http/http_stream.h>
#include <gqlxy/internal/http/https_stream.h>
#include <gqlxy/internal/url.h>
#include <memory>
#include <rpp/sources/create.hpp>
#include <string>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace boost::asio;
namespace http = boost::beast::http;
using namespace rpp;
using namespace rpp::source;

HttpLink::HttpLink(const HttpLinkOptions& options) : _options(options) {}

shared_ptr<IHttpStream> CreateStream(const any_io_executor& executor, const Url& url, const optional<string>& caCert) {
    return url.tls ? static_pointer_cast<IHttpStream>(make_shared<HttpsStream>(executor, url, caCert))
                   : make_shared<HttpStream>(executor, url);
}

Observable<GraphQLResponse> HttpLink::Execute(const GraphQLRequest& request) {
    return create<GraphQLResponse>([opts = _options, request](auto&& sub) {
        auto subscription = make_shared<dynamic_observer<GraphQLResponse>>(std::move(sub));
        try {
            const auto url = ParseHttpUrl(opts.url);
            auto stream = CreateStream(AsioContext::Get().get_executor(), url, opts.caCert);
            stream->Send(request, opts.headers).subscribe(
                [subscription, stream](const GraphQLResponse& r) { subscription->on_next(r); },
                [subscription, stream](const exception_ptr& e) { subscription->on_error(e); },
                [subscription, stream]() { subscription->on_completed(); });
        } catch (...) {
            subscription->on_error(current_exception());
        }
    });
}
