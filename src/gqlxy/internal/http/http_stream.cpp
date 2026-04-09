#include <gqlxy/internal/http/http_stream.h>

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

using namespace std;
using namespace gqlxy::internal;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace boost::beast;
using namespace boost::beast::http;
namespace beast = boost::beast;

HttpStream::HttpStream(const any_io_executor& ex) : _stream(ex) {}

awaitable<void> HttpStream::Connect(const string& host, const string& port) {
    co_await _stream.async_connect(
        co_await tcp::resolver(_stream.get_executor()).async_resolve(host, port, use_awaitable), use_awaitable);
}

awaitable<void> HttpStream::Write(const request<string_body>& req) {
    co_await async_write(_stream, req, use_awaitable);
}

awaitable<void> HttpStream::Read(flat_buffer& buf, response<string_body>& res) {
    co_await async_read(_stream, buf, res, use_awaitable);
}

awaitable<void> HttpStream::Shutdown() {
    beast::error_code ec;
    _stream.socket().shutdown(tcp::socket::shutdown_both, ec);
    co_return;
}