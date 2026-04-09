#include <gqlxy/links/http_link.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/beast/http.hpp>
#include <gqlxy/internal/asio_context.h>
#include <gqlxy/internal/http/http_stream.h>
#include <gqlxy/internal/http/https_stream.h>
#include <gqlxy/internal/http/response.h>
#include <gqlxy/internal/http/serialization.h>
#include <gqlxy/internal/url.h>
#include <map>
#include <string>
#include <vector>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
namespace net = boost::asio;
namespace http = boost::beast::http;
using namespace rxcpp;
using net::awaitable;

awaitable<vector<GraphQLResult>> SendAndReceive(
    IHttpStream& stream, const string& host, const string& target, const string& body,
    const map<string, string>& extra_headers) {
    http::request<http::string_body> req {http::verb::post, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::content_type, "application/json");
    req.set(http::field::accept, "application/json, text/event-stream");
    req.set(http::field::user_agent, "gqlxy-client/0.1");
    for (const auto& [k, v] : extra_headers)
        req.set(k, v);
    req.body() = body;
    req.prepare_payload();

    co_await stream.Write(req);

    boost::beast::flat_buffer buf;
    http::response<http::string_body> res;
    co_await stream.Read(buf, res);

    if (const auto status = res.result(); status >= http::status::bad_request)
        co_return vector {MapHttpError(status, res.reason())};

    if (res[http::field::content_type].find("text/event-stream") != string::npos) co_return ParseSseBody(res.body());

    co_return vector {ParseJsonResponse(res.body())};
}

awaitable<vector<GraphQLResult>> HttpRequest(const HttpLinkOptions& opts, const GraphQLRequest& req) {
    const auto url = ParseHttpUrl(opts.url);
    const auto body = SerializeRequest(req).dump();

    auto executor = co_await net::this_coro::executor;
    auto stream = url.tls ? static_pointer_cast<IHttpStream>(make_shared<HttpsStream>(executor))
                          : make_shared<HttpStream>(executor);

    co_await stream->Connect(url.host, url.port);
    auto results = co_await SendAndReceive(*stream, url.host, url.target, body, opts.headers);
    co_await stream->Shutdown();
    co_return results;
}

HttpLink::HttpLink(const HttpLinkOptions& options) : _options(options) {}

Observable<GraphQLResult> HttpLink::Execute(const GraphQLRequest& request) {
    return observable<>::create<GraphQLResult>([opts = _options, request](const auto& subscription) {
        co_spawn(AsioContext::Get(), HttpRequest(opts, request), [subscription](auto exception, auto results) {
            if (!subscription.is_subscribed()) return;
            if (exception) return subscription.on_error(exception);
            for (const auto& result : results)
                subscription.on_next(result);
            subscription.on_completed();
        });
    });
}
