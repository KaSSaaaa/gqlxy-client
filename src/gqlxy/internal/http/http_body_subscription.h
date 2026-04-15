#pragma once
#include <boost/asio/steady_timer.hpp>
#include <rxcpp/rx.hpp>

namespace gqlxy {
struct GraphQLResult;
}

namespace gqlxy::internal {
struct HttpBodyChunk;

class HttpBodySubscription {
public:
    HttpBodySubscription(boost::asio::steady_timer& done, rxcpp::subscriber<GraphQLResult>& subscriber, const bool isSse);

    void Subscribe(const rxcpp::observable<HttpBodyChunk>& observable);

    void Unsubscribe();

private:
    boost::asio::steady_timer& _done;
    rxcpp::composite_subscription _subscription;
    rxcpp::subscriber<GraphQLResult>& _subscriber;
    const bool _isSse;

    bool _completed = false;
    std::string _body;

    void HandleChunk(const HttpBodyChunk& chunk);
    void HandleError(const std::exception_ptr& e);
    void Finally();
    void Stop();
    void Complete();
    void HandleJsonChunk(const HttpBodyChunk& chunk);
    void HandleSseChunk(const HttpBodyChunk& chunk);
};

}