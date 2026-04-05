#include "url.h"

using namespace std;
using namespace boost::urls;

namespace gqlxy::internal {

ParsedUrl ParseUrl(const string& url, const vector<scheme>& allowedSchemes) {
    auto r = parse_uri(url);
    if (!r) throw invalid_argument(std::format("Invalid GraphQL endpoint URL: {}", url));

    const url_view u = r.value();
    if (!ranges::any_of(allowedSchemes, [&u](const scheme& s) { return s == u.scheme_id(); }))
        throw invalid_argument(std::format("Unsupported URL scheme '{}': ", u.scheme(), url));

    const auto tls = u.scheme_id() == scheme::https || u.scheme_id() == scheme::wss;
    const auto path = string(u.encoded_path());
    return {
        .tls = tls,
        .host = string(u.host()),
        .port = u.has_port() ? string(u.port()) : (tls ? "443" : "80"),
        .target = path.empty() ? "/" : path,
    };
}

ParsedUrl ParseHttpUrl(const std::string& url) {
    return ParseUrl(url, {scheme::http, scheme::https});
}

ParsedUrl ParseWsUrl(const std::string& url) {
    return ParseUrl(url, {scheme::ws, scheme::wss});
}

}