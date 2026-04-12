#include "https_stream.h"

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/write.hpp>

using namespace std;
using namespace gqlxy::internal;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::beast;
using namespace boost::beast::http;
namespace beast = boost::beast;

HttpsStream::HttpsStream(const any_io_executor& ex, const optional<string>& caCert)
    : _ctx(ssl::context::tlsv13_client),
      _stream(ex, _ctx) {
    if (caCert) _ctx.add_certificate_authority(buffer(*caCert));
    else _ctx.set_default_verify_paths();
    _ctx.set_verify_mode(ssl::verify_peer);
}

awaitable<void> HttpsStream::Connect(const string& host, const string& port) {
    SSL_set_tlsext_host_name(_stream.native_handle(), host.c_str());
    co_await get_lowest_layer(_stream).async_connect(
        co_await tcp::resolver(get_lowest_layer(_stream).get_executor()).async_resolve(host, port, use_awaitable),
        use_awaitable);
    co_await _stream.async_handshake(ssl::stream_base::client, use_awaitable);
}

awaitable<void> HttpsStream::Write(const request<string_body>& req) {
    co_await async_write(_stream, req, use_awaitable);
}
awaitable<void> HttpsStream::ReadHeader(flat_buffer& buf, response_parser<string_body>& parser) {
    co_await async_read_header(_stream, buf, parser, use_awaitable);
}
awaitable<bool> HttpsStream::ReadBodyChunk(flat_buffer& buf, response_parser<string_body>& parser) {
    if (parser.is_done()) co_return false;
    beast::error_code ec;
    co_await async_read_some(_stream, buf, parser, redirect_error(use_awaitable, ec));
    if (ec == net::error::eof || ec == http::error::end_of_stream) co_return false;
    if (ec) throw boost::system::system_error(ec);
    co_return !parser.is_done();
}
awaitable<void> HttpsStream::Shutdown() {
    beast::error_code ec;
    co_await _stream.async_shutdown(redirect_error(use_awaitable, ec));
    co_return;
}