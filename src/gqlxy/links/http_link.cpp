#include <gqlxy/links/http_link.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/http.hpp>
#include <gqlxy/internal/asio_context.h>
#include <gqlxy/internal/http/http_body_subscription.h>
#include <gqlxy/internal/http/http_stream.h>
#include <gqlxy/internal/http/https_stream.h>
#include <gqlxy/internal/http/response.h>
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

shared_ptr<IHttpStream> CreateStream(const any_io_executor& executor, const Url& url, const optional<string>& caCert) {
    return url.tls ? static_pointer_cast<IHttpStream>(make_shared<HttpsStream>(executor, caCert))
                   : make_shared<HttpStream>(executor);
}

http::request<http::string_body>
BuildRequest(const Url& url, const string& body, const HttpLinkOptions& opts, const GraphQLRequest& request) {
    http::request<http::string_body> req {http::verb::post, url.target, 11};
    req.set(http::field::host, url.host);
    string accept = "application/json";
    req.set(http::field::content_type, accept);
    if (request.type == OperationType::Subscription) accept += ",text/event-stream";
    req.set(http::field::accept, accept);
    req.set(http::field::user_agent, "gqlxy-client/0.1");
    for (const auto& [k, v] : opts.headers)
        req.set(k, v);
    req.body() = body;
    req.prepare_payload();
    return req;
}

awaitable<void> ReadResponse(bool isSse, shared_ptr<IHttpStream> stream, subscriber<GraphQLResult> sub) {
    steady_timer done(co_await this_coro::executor, steady_timer::time_point::max());

    HttpBodySubscription s(done, sub, isSse);
    s.Subscribe(stream->Read());

    boost::beast::error_code ec;
    co_await done.async_wait(redirect_error(use_awaitable, ec));
    s.Unsubscribe();
    co_await stream->Shutdown();
}

awaitable<void> HttpRequest(HttpLinkOptions opts, GraphQLRequest req, subscriber<GraphQLResult> sub) {
    try {
        const auto url = ParseHttpUrl(opts.url);
        auto stream = CreateStream(co_await this_coro::executor, url, opts.caCert);
        co_await stream->Connect(url.host, url.port);
        co_await stream->Write(BuildRequest(url, SerializeRequest(req).dump(), opts, req));
        co_await ReadResponse(req.type == OperationType::Subscription, stream, sub);
    } catch (...) {
        if (sub.is_subscribed()) sub.on_error(current_exception());
    }
}

HttpLink::HttpLink(const HttpLinkOptions& options) : _options(options) {}

Observable<GraphQLResult> HttpLink::Execute(const GraphQLRequest& request) {
    return observable<>::create<GraphQLResult>([opts = _options, request](const auto& sub) {
        co_spawn(AsioContext::Get(), HttpRequest(opts, request, sub), [sub](const exception_ptr& ep) {
            if (ep && sub.is_subscribed()) sub.on_error(ep);
        });
    });
}
