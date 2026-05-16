#pragma once

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <gqlxy/client/internal/http/i_http_stream.h>
#include <gqlxy/client/internal/http/response.h>
#include <gqlxy/client/internal/http/serialization.h>
#include <gqlxy/client/internal/url.h>
#include <gqlxy/core/results.h>
#include <rpp/sources/create.hpp>

namespace gqlxy::internal {

template<typename Stream>
class HttpStreamBase : public IHttpStream {
public:
    rpp::dynamic_observable<GraphQLResponse> Send(const GraphQLRequest& request, const Headers& headers) override {
        return rpp::source::create<GraphQLResponse>([this, request, headers](auto sub) {
            boost::asio::co_spawn(
                boost::beast::get_lowest_layer(_stream).get_executor(),
                Send(BuildRequest(request, headers), request.type._value == parser::OperationType::SUBSCRIPTION, std::move(sub)),
                boost::asio::detached);
        }).as_dynamic();
    }

protected:
    Stream _stream;
    Url _url;
    boost::beast::flat_buffer _buf;
    boost::beast::http::response_parser<boost::beast::http::string_body> _parser;

    HttpStreamBase(Stream stream, const Url& url) : _stream(std::move(stream)), _url(url) {
        _parser.body_limit(boost::none);
    }

    virtual boost::asio::awaitable<void> Connect(const std::string& host, const std::string& port) = 0;
    virtual boost::asio::awaitable<void> Shutdown() = 0;

private:
    boost::asio::awaitable<void> Write(const boost::beast::http::request<boost::beast::http::string_body>& req) {
        co_await boost::beast::http::async_write(_stream, req, boost::asio::use_awaitable);
    }

    boost::beast::http::request<boost::beast::http::string_body> BuildRequest(const GraphQLRequest& request, const Headers& headers) {
        boost::beast::http::request<boost::beast::http::string_body> req {boost::beast::http::verb::post, _url.target, 11};
        req.set(boost::beast::http::field::host, _url.host);
        std::string accept = "application/json";
        req.set(boost::beast::http::field::content_type, accept);
        if (request.type._value == parser::OperationType::SUBSCRIPTION) accept += ",text/event-stream";
        req.set(boost::beast::http::field::accept, accept);
        req.set(boost::beast::http::field::user_agent, "gqlxy-client/0.1");
        for (const auto& [k, v] : headers)
            req.set(k, v);
        req.body() = SerializeRequest(request).dump();
        req.prepare_payload();
        return req;
    }

    boost::asio::awaitable<void> Send(boost::beast::http::request<boost::beast::http::string_body> req, bool isSse,
                                      rpp::dynamic_observer<GraphQLResponse> sub) {
        try {
            co_await Connect(_url.host, _url.port);
            co_await Write(req);

            boost::asio::steady_timer done(co_await boost::asio::this_coro::executor, boost::asio::steady_timer::time_point::max());

            Read(isSse).subscribe(
                [&sub](const GraphQLResponse& r) { sub.on_next(r); },
                [&sub, &done](const std::exception_ptr& e) {
                    sub.on_error(e);
                    done.cancel();
                },
                [&sub, &done]() {
                    sub.on_completed();
                    done.cancel();
                });

            boost::beast::error_code ec;
            co_await done.async_wait(boost::asio::redirect_error(boost::asio::use_awaitable, ec));
            co_await Shutdown();
        } catch (...) {
            sub.on_error(std::current_exception());
        }
    }

    rpp::dynamic_observable<GraphQLResponse> Read(bool isSse) {
        return rpp::source::create<GraphQLResponse>([this, isSse](auto sub) {
            boost::asio::co_spawn(
                boost::beast::get_lowest_layer(_stream).get_executor(), Read(std::move(sub), isSse),
                boost::asio::detached);
        }).as_dynamic();
    }

    boost::asio::awaitable<void> Read(rpp::dynamic_observer<GraphQLResponse> sub, bool isSse) {
        try {
            if (!co_await IsPositiveReply(sub)) co_return;
            co_await (isSse ? OnSseResponse(sub) : OnResponse(sub));
        } catch (const boost::system::system_error& e) {
            if (e.code() != boost::asio::error::eof && e.code() != boost::beast::http::error::end_of_stream)
                sub.on_error(std::current_exception());
        } catch (...) {
            sub.on_error(std::current_exception());
        }
        sub.on_completed();
    }

    boost::asio::awaitable<bool> IsPositiveReply(rpp::dynamic_observer<GraphQLResponse>& sub) {
        co_await boost::beast::http::async_read_header(_stream, _buf, _parser, boost::asio::use_awaitable);
        const auto& response = _parser.get().base();
        if (response.result() < boost::beast::http::status::bad_request) co_return true;
        sub.on_next(ConvertHttpError(response.result(), response.reason()));
        sub.on_completed();
        co_return false;
    }

    boost::asio::awaitable<void> OnSseResponse(const rpp::dynamic_observer<GraphQLResponse>& sub) {
        std::string payload;
        while (!_parser.is_done() && !sub.is_disposed()) {
            co_await ReadAsync();
            if (auto& body = _parser.get().body(); !body.empty()) {
                payload += body;
                body.clear();
                auto [results, remaining, completed] = ParseSseEvents(payload);
                payload = std::move(remaining);
                for (const auto& result : results)
                    sub.on_next(result);

                if (completed) co_return;
            }
        }
    }

    boost::asio::awaitable<void> OnResponse(const rpp::dynamic_observer<GraphQLResponse>& sub) {
        while (!_parser.is_done())
            co_await ReadAsync();
        sub.on_next(ParseJsonPayload(_parser.get().body()));
    }

    boost::asio::awaitable<void> ReadAsync() {
        co_await boost::beast::http::async_read_some(_stream, _buf, _parser, boost::asio::use_awaitable);
    }
};

}
