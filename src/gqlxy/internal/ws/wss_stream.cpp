#include "wss_stream.h"

#include <boost/asio/ssl/stream.hpp>
#include <gqlxy/internal/asio_context.h>
#include <gqlxy/internal/ssl_context.h>

using namespace std;
using namespace gqlxy::internal;
using namespace boost::asio::ssl;
using namespace boost::beast;
namespace beast = boost::beast;

WssStream::WssStream(const Url& url, const map<string, string>& headers, const optional<string>& caCert)
    : WssStream(url, headers, make_unique<context>(CreateSslContext(caCert))) {}

WssStream::WssStream(const Url& url, const map<string, string>& headers, unique_ptr<context> ctx)
    : WsStreamBase(url, headers, websocket::stream<stream<tcp_stream>>(AsioContext::Get(), *ctx)),
      _ctx(std::move(ctx)) {}

void WssStream::Handshake(const ConnectCallback& callback) {
    SSL_set_tlsext_host_name(_ws.next_layer().native_handle(), _url.host.c_str());
    _ws.next_layer().async_handshake(stream_base::client, [this, callback](const auto& ec) {
        if (ec) return callback(ec);
        WsStreamBase::Handshake(callback);
    });
}
