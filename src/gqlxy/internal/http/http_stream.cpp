#include <gqlxy/internal/http/http_stream.h>

#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>

using namespace std;
using namespace gqlxy::internal;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::beast;

HttpStream::HttpStream(const any_io_executor& ex) : HttpStreamBase(tcp_stream(ex)) {}

awaitable<void> HttpStream::Connect(const string& host, const string& port) {
    co_await _stream.async_connect(
        co_await tcp::resolver(_stream.get_executor()).async_resolve(host, port, use_awaitable), use_awaitable);
}

awaitable<void> HttpStream::Shutdown() {
    boost::beast::error_code ec;
    _stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    co_return;
}