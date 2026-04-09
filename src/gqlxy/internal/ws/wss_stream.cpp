#include "wss_stream.h"

#include <boost/asio/ssl/stream.hpp>
#include <gqlxy/internal/asio_context.h>

using namespace std;
using namespace gqlxy::internal;
using namespace boost::asio::ssl;
using namespace boost::beast;
namespace beast = boost::beast;

WssStream::WssStream(const ParsedUrl& url, const map<string, string>& headers, const WsTransportCallbacks& callbacks)
    : WssStream(url, headers, callbacks, MakeSslCtx()) {}

WssStream::WssStream(
    const ParsedUrl& url, const map<string, string>& headers, const WsTransportCallbacks& callbacks,
    unique_ptr<context> ctx)
    : WsStreamBase(url, headers, callbacks, websocket::stream<stream<tcp_stream>>(AsioContext::Get(), *ctx)),
      _ctx(std::move(ctx)) {}

unique_ptr<context> WssStream::MakeSslCtx() {
    auto ctx = make_unique<context>(context::tlsv13_client);
    ctx->set_default_verify_paths();
    ctx->set_verify_mode(verify_peer);
    return ctx;
}

void WssStream::Handshake(const ConnectCallback& callback) {
    SSL_set_tlsext_host_name(_ws.next_layer().native_handle(), _url.host.c_str());
    WsStreamBase::Handshake(callback);
}
