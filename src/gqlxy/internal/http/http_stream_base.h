#pragma once

#include "i_http_stream.h"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <limits>

//TODO Simplify

namespace gqlxy::internal {

template<typename Stream>
class HttpStreamBase : public IHttpStream {
public:
    boost::asio::awaitable<void>
    Write(const boost::beast::http::request<boost::beast::http::string_body>& req) override {
        co_await boost::beast::http::async_write(_stream, req, boost::asio::use_awaitable);
    }

    rxcpp::observable<HttpBodyChunk> Read() override {
        return rxcpp::observable<>::create<HttpBodyChunk>([this](auto sub) {
            boost::asio::co_spawn(
                boost::beast::get_lowest_layer(_stream).get_executor(), Read(std::move(sub)), boost::asio::detached);
        });
    }

protected:
    HttpStreamBase(Stream stream) : _stream(std::move(stream)) {
        _parser.body_limit(std::numeric_limits<uint64_t>::max());
    }

    Stream _stream;
    boost::beast::flat_buffer _buf;
    boost::beast::http::response_parser<boost::beast::http::string_body> _parser;

private:
    boost::asio::awaitable<void> Read(rxcpp::subscriber<HttpBodyChunk>&& subscriber) {
        const auto sub = std::move(subscriber);
        try {
            co_await boost::beast::http::async_read_header(_stream, _buf, _parser, boost::asio::use_awaitable);
            sub.on_next(HttpBodyChunk {_parser.get().base(), {}, false});

            size_t processedSize = 0;
            while (!_parser.is_done() && sub.is_subscribed()) {
                try {
                    co_await boost::beast::http::async_read_some(_stream, _buf, _parser, boost::asio::use_awaitable);
                } catch (const boost::system::system_error& e) {
                    if (e.code() == boost::asio::error::eof || e.code() == boost::beast::http::error::end_of_stream)
                        break;
                    if (sub.is_subscribed()) sub.on_error(std::current_exception());
                    co_return;
                }
                const auto& body = _parser.get().body();
                if (body.size() > processedSize) {
                    sub.on_next(HttpBodyChunk {std::nullopt, body.substr(processedSize), _parser.is_done()});
                    processedSize = body.size();
                }
            }
            if (sub.is_subscribed()) sub.on_completed();
        } catch (...) {
            if (sub.is_subscribed()) sub.on_error(std::current_exception());
        }
    }
};

}
