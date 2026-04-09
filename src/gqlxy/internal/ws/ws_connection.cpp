#include <boost/asio/post.hpp>
#include <future>
#include <gqlxy/internal/asio_context.h>
#include <gqlxy/internal/ws/ws_connection.h>

using namespace std;
using namespace gqlxy::internal;
using namespace boost::asio;
using namespace rxcpp;

WsConnection::WsConnection(const WsLinkOptions& opts) : _ctx(make_shared<WsConnectionContext>(opts)) {}

WsConnection::~WsConnection() {
    Stop();
}

void WsConnection::Stop() {
    if (AsioContext::OnContext()) return _ctx->Stop();
    promise<void> done;
    post(AsioContext::Get(), [ctx = _ctx, &done]() {
        ctx->Stop();
        post(AsioContext::Get(), [&done]() { done.set_value(); });
    });
    done.get_future().get();
}

void WsConnection::Subscribe(const string& id, const GraphQLRequest& req, const subscriber<GraphQLResult>& sub) {
    post(AsioContext::Get(), [ctx = _ctx, id, req, sub]() {
        if (ctx->InitError()) return sub.on_error(ctx->InitError());
        if (ctx->IsStopping()) return sub.on_completed();
        ctx->DispatchSubscribe(id, req, sub);
    });
}

void WsConnection::Unsubscribe(const string& id) {
    post(AsioContext::Get(), [ctx = _ctx, id]() { ctx->DispatchUnsubscribe(id); });
}
