#include <gqlxy/links/http_link.h>

#include <gqlxy/internal/http/response.h>
#include <gqlxy/internal/http/serialization.h>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/url.hpp>
#include <openssl/ssl.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;
using namespace gqlxy;
using namespace gqlxy::internal;
using namespace boost;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::beast;
using namespace boost::urls;
using namespace rxcpp;

struct ParsedUrl {
    bool tls;
    string host;
    string port;
    string target;
};

ParsedUrl parse_url(const string& url) {
    auto r = parse_uri(url);
    if (!r) throw invalid_argument("Invalid GraphQL endpoint URL: " + url);

    const url_view u = r.value();
    if (u.scheme_id() != scheme::http && u.scheme_id() != scheme::https)
        throw invalid_argument("Unsupported URL scheme '" + string(u.scheme()) + "': " + url);

    const auto tls = u.scheme_id() == scheme::https;
    const auto path = string(u.encoded_path());
    return {
        .tls = tls,
        .host = string(u.host()),
        .port = u.has_port() ? string(u.port()) : (tls ? "443" : "80"),
        .target = path.empty() ? "/" : path,
    };
}

template<typename Stream>
vector<GraphQLResult> send_and_receive(
    Stream& stream, const string& host, const string& target, const string& body_str,
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

    http::write(stream, req);

    flat_buffer buf;
    http::response<http::string_body> res;
    http::read(stream, buf, res);

    if (const auto status = res.result_int(); status >= 400) return {map_http_error(status, res.reason())};

    if (res[http::field::content_type].find("text/event-stream") != string::npos) return parse_sse_body(res.body());

    return {parse_json_response(res.body())};
}

vector<GraphQLResult> do_http(const ParsedUrl& url, const string& body_str, const map<string, string>& headers) {
    io_context ioc;
    tcp::resolver resolver {ioc};
    tcp_stream stream {ioc};
    stream.connect(resolver.resolve(url.host, url.port));
    auto results = send_and_receive(stream, url.host, url.target, body_str, headers);
    beast::error_code ec;
    stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    return results;
}

vector<GraphQLResult> do_https(const ParsedUrl& url, const string& body_str, const map<string, string>& headers) {
    io_context ioc;
    ssl::context ctx {ssl::context::tlsv12_client};
    ctx.set_default_verify_paths();
    ctx.set_verify_mode(ssl::verify_peer);

    ssl_stream<tcp_stream> stream {ioc, ctx};
    SSL_set_tlsext_host_name(stream.native_handle(), url.host.c_str());
    get_lowest_layer(stream).connect(tcp::resolver {ioc}.resolve(url.host, url.port));
    stream.handshake(ssl::stream_base::client);

    auto results = send_and_receive(stream, url.host, url.target, body_str, headers);
    beast::error_code ec;
    stream.shutdown(ec);
    return results;
}

HttpLink::HttpLink(const HttpLinkOptions& options) : _options(options) {}

Observable<GraphQLResult> HttpLink::Execute(const GraphQLRequest& request) {
    return observable<>::create<GraphQLResult>([opts = _options, req = request](const auto& s) {
        try {
            const auto url = parse_url(opts.url);
            const auto body = serialize_request(req).dump();
            auto results = url.tls ? do_https(url, body, opts.headers) : do_http(url, body, opts.headers);
            for (auto& r : results) {
                if (!s.is_subscribed()) break;
                s.on_next(std::move(r));
            }
            s.on_completed();
        } catch (...) {
            s.on_error(current_exception());
        }
    });
}
