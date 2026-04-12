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
#include <limits>
#include <map>
#include <string>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
namespace net = boost::asio;
namespace http = boost::beast::http;
using namespace rxcpp;
using net::awaitable;

awaitable<void> HttpRequest(HttpLinkOptions opts, GraphQLRequest req, rxcpp::subscriber<GraphQLResult> sub) {
    try {
        const auto url = ParseHttpUrl(opts.url);
        const auto body = SerializeRequest(req).dump();

        auto executor = co_await net::this_coro::executor;
        auto stream = url.tls ? static_pointer_cast<IHttpStream>(make_shared<HttpsStream>(executor, opts.caCert))
                              : make_shared<HttpStream>(executor);

        co_await stream->Connect(url.host, url.port);

        http::request<http::string_body> httpReq {http::verb::post, url.target, 11};
        httpReq.set(http::field::host, url.host);
        httpReq.set(http::field::content_type, "application/json");
        httpReq.set(http::field::accept, "application/json, text/event-stream");
        httpReq.set(http::field::user_agent, "gqlxy-client/0.1");
        for (const auto& [k, v] : opts.headers)
            httpReq.set(k, v);
        httpReq.body() = body;
        httpReq.prepare_payload();

        co_await stream->Write(httpReq);

        boost::beast::flat_buffer buf;
        http::response_parser<http::string_body> parser;
        parser.body_limit(numeric_limits<uint64_t>::max());

        co_await stream->ReadHeader(buf, parser);

        const auto status = parser.get().result();
        if (status >= http::status::bad_request) {
            sub.on_next(MapHttpError(status, parser.get().reason()));
            sub.on_completed();
            co_return;
        }

        const bool isSse = parser.get()[http::field::content_type].find("text/event-stream") != string::npos;

        if (!isSse) {
            while (co_await stream->ReadBodyChunk(buf, parser))
                ;
            if (sub.is_subscribed()) {
                sub.on_next(ParseJsonResponse(parser.get().body()));
                sub.on_completed();
            }
        } else {
            string pending;
            size_t processedSize = 0;

            while (sub.is_subscribed()) {
                const bool more = co_await stream->ReadBodyChunk(buf, parser);
                const auto& totalBody = parser.get().body();

                if (totalBody.size() > processedSize) {
                    pending += totalBody.substr(processedSize);
                    processedSize = totalBody.size();

                    auto [results, remaining, completed] = DrainSseEvents(pending);
                    pending = std::move(remaining);

                    for (auto& r : results) {
                        if (!sub.is_subscribed()) co_return;
                        sub.on_next(std::move(r));
                    }

                    if (completed) break;
                }

                if (!more) break;
            }

            if (sub.is_subscribed()) sub.on_completed();
        }

        co_await stream->Shutdown();
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
