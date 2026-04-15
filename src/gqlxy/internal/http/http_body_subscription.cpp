#include "http_body_subscription.h"

#include "i_http_stream.h"
#include "response.h"

using namespace std;
using namespace gqlxy::internal;
using namespace boost::asio;
using namespace boost::beast;
using namespace rxcpp;

HttpBodySubscription::HttpBodySubscription(steady_timer& done, subscriber<GraphQLResult>& subscriber, const bool isSse)
    : _done(done),
      _subscriber(subscriber),
      _isSse(isSse) {}

void HttpBodySubscription::Subscribe(const observable<HttpBodyChunk>& observable) {
    observable.subscribe(make_subscriber<HttpBodyChunk>(_subscription,
        [this](const HttpBodyChunk& chunk) { HandleChunk(chunk); },
        [this](const auto& ep) { HandleError(ep); },
        [this]() { Finally(); })
    );
}

void HttpBodySubscription::Unsubscribe() {
    _subscription.unsubscribe();
}

void HttpBodySubscription::Finally() {
    if (!_completed && _subscriber.is_subscribed()) {
        if (!_isSse) _subscriber.on_next(ParseJsonResponse(_body));
        _subscriber.on_completed();
    }
    _done.cancel();
}

void HttpBodySubscription::Stop() {
    _subscription.unsubscribe();
    _done.cancel();
}

void HttpBodySubscription::Complete() {
    _subscriber.on_completed();
    _completed = true;
    Stop();
}

void HttpBodySubscription::HandleChunk(const HttpBodyChunk& chunk) {
    if (!_subscriber.is_subscribed()) return Stop();
    if (chunk.header) {
        if (const auto& header = *chunk.header; header.result() >= http::status::bad_request) {
            _subscriber.on_next(MapHttpError(header.result(), header.reason()));
            return Complete();
        }
    }
    _isSse ? HandleSseChunk(chunk) : HandleJsonChunk(chunk);
}

void HttpBodySubscription::HandleError(const std::exception_ptr& e) {
    if (_subscriber.is_subscribed()) _subscriber.on_error(e);
    _done.cancel();
}

void HttpBodySubscription::HandleJsonChunk(const HttpBodyChunk& chunk) {
    _body += chunk.data;
    if (chunk.done) {
        _subscriber.on_next(ParseJsonResponse(_body));
        Complete();
    }
}

void HttpBodySubscription::HandleSseChunk(const HttpBodyChunk& chunk) {
    _body += chunk.data;
    auto [results, remaining, sseCompleted] = ParseSseEvents(_body);
    _body = std::move(remaining);
    for (auto& r : results)
        _subscriber.on_next(std::move(r));
    if (sseCompleted) Complete();
}