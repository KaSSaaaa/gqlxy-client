#include <gqlxy/links/http_link.h>

#include <gqlxy/internal/asio_context.h>
#include <gqlxy/internal/http/response.h>
#include <gqlxy/internal/http/serialization.h>
#include <gqlxy/internal/url.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <openssl/ssl.h>

#include <map>
#include <string>
#include <vector>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::beast;
using namespace rxcpp;

template<typename Stream>
awaitable<vector<GraphQLResult>> SendAndReceive(Stream& stream,
                                                const string& host,
                                                const string& target,
                                                const string& body_str,
                                                const map<string, string>& extra_headers) {
    http::request<http::string_body> req {http::verb::post, target, 11};
    req.set(http::field::host, host);
    req.set(http::field::content_type, "application/json");
    req.set(http::field::accept, "application/json, text/event-stream");
    req.set(http::field::user_agent, "gqlxy-client/0.1");
    for (const auto& [k, v] : extra_headers)
        req.set(k, v);
    req.body() = body_str;
    req.prepare_payload();

    co_await http::async_write(stream, req, use_awaitable);

    flat_buffer buf;
    http::response<http::string_body> res;
    co_await http::async_read(stream, buf, res, use_awaitable);

    if (const auto status = res.result(); status >= http::status::bad_request)
        co_return vector {MapHttpError(status, res.reason())};

    if (res[http::field::content_type].find("text/event-stream") != string::npos)
        co_return ParseSseBody(res.body());

    co_return vector {ParseJsonResponse(res.body())};
}

awaitable<vector<GraphQLResult>> Send(const ParsedUrl& url, const string& body, const map<string, string>& headers) {
    auto ex = co_await this_coro::executor;
    tcp_stream stream(ex);
    co_await stream.async_connect(co_await tcp::resolver(ex).async_resolve(url.host, url.port, use_awaitable), use_awaitable);

    auto results = co_await SendAndReceive(stream, url.host, url.target, body, headers);
    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    co_return results;
}

awaitable<vector<GraphQLResult>> SendSecured(const ParsedUrl& url, const string& body, const map<string, string>& headers) {
    auto ex = co_await this_coro::executor;
    ssl::context ctx {ssl::context::tlsv13_client};
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);

    ssl_stream<tcp_stream> stream {ex, ctx};
    SSL_set_tlsext_host_name(stream.native_handle(), url.host.c_str());

    co_await get_lowest_layer(stream).async_connect(co_await tcp::resolver(ex).async_resolve(url.host, url.port, use_awaitable), use_awaitable);
    co_await stream.async_handshake(ssl::stream_base::client, use_awaitable);

    auto results = co_await SendAndReceive(stream, url.host, url.target, body, headers);
    beast::error_code ec;
    co_await stream.async_shutdown(redirect_error(use_awaitable, ec));
    co_return results;
}

awaitable<vector<GraphQLResult>> HttpRequest(const HttpLinkOptions& opts, const GraphQLRequest& req) {
    const auto url = ParseHttpUrl(opts.url);
    const auto body = SerializeRequest(req).dump();
    co_return co_await (url.tls ? SendSecured(url, body, opts.headers) : Send(url, body, opts.headers));
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
