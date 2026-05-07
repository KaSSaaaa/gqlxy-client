#include "https_stream.h"

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/ssl.hpp>
#include <gqlxy/internal/ssl_context.h>

using namespace std;
using namespace gqlxy::internal;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::beast;
namespace beast = boost::beast;

HttpsStream::HttpsStream(const any_io_executor& ex, const Url& url, const optional<string>& caCert)
    : HttpsStream(ex, url, CreateSslContext(caCert)) {}

HttpsStream::HttpsStream(const any_io_executor& ex, const Url& url, unique_ptr<ssl::context> ctx)
    : HttpStreamBase(ssl_stream<tcp_stream>(ex, *ctx), url),
      _ctx(std::move(ctx)) {}

awaitable<void> HttpsStream::Connect(const string& host, const string& port) {
    SSL_set_tlsext_host_name(_stream.native_handle(), host.c_str());
    co_await get_lowest_layer(_stream).async_connect(
        co_await tcp::resolver(get_lowest_layer(_stream).get_executor()).async_resolve(host, port, use_awaitable),
        use_awaitable);
    co_await _stream.async_handshake(ssl::stream_base::client, use_awaitable);
}

awaitable<void> HttpsStream::Shutdown() {
    beast::error_code ec;
    co_await _stream.async_shutdown(redirect_error(use_awaitable, ec));
    co_return;
}