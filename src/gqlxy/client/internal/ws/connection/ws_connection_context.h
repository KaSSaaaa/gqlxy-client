#pragma once

#include <gqlxy/client/internal/url.h>
#include <gqlxy/client/links/ws_link.h>

#include <boost/asio/steady_timer.hpp>
#include <rpp/observers/dynamic_observer.hpp>

#include <atomic>
#include <exception>
#include <map>
#include <memory>
#include <string>

namespace gqlxy::internal {

struct WsSubscription {
    GraphQLRequest request;
    rpp::dynamic_observer<GraphQLResponse> subscriber;
};

class WsTransport;

enum class ConnectionState {
    Idle,
    Connecting,
    Connected,
    Reconnecting
};

class WsConnectionContext : public std::enable_shared_from_this<WsConnectionContext> {
public:
    explicit WsConnectionContext(const WsLinkOptions& opts);
    ~WsConnectionContext();

    void Subscribe(const std::string& id, const GraphQLRequest& req, const rpp::dynamic_observer<GraphQLResponse>& sub);
    void Unsubscribe(const std::string& id);
    void Stop();

private:
    void OnSubscribe(const std::string& id, const GraphQLRequest& req, const rpp::dynamic_observer<GraphQLResponse>& sub);
    void OnUnsubscribe(const std::string& id);
    void OnTransportConnected();
    void OnTransportMessage(const std::string& raw);
    void OnTransportDisconnected();
    void TransitionTo(ConnectionState state);

    void AddSub(const std::string& id, const GraphQLRequest& req, const rpp::dynamic_observer<GraphQLResponse>& sub);
    void RemoveSub(const std::string& id);
    bool HasSubs() const;
    void ReplaySubs();
    void FailAll(const std::exception_ptr& ex);
    void CompleteSub(const std::string& id);

    void ConnectTransport();
    void Send(const nlohmann::json& msg);
    void SendSubscribe(const std::string& id, const GraphQLRequest& req);
    void Dispatch(const std::string& raw);

    void ScheduleReconnect();
    void CancelReconnect();

    void StopOnContext();

    template <typename F>
    auto WeakCallback(F&& fn) {
        return [weak = weak_from_this(), fn = std::forward<F>(fn)]<typename... TArgs>(TArgs&&... args) {
            if (auto ctx = weak.lock()) fn(*ctx, std::forward<TArgs>(args)...);
        };
    }

    ConnectionState _state = ConnectionState::Idle;
    std::map<std::string, WsSubscription> _subs;
    std::shared_ptr<WsTransport> _transport;
    boost::asio::steady_timer _reconnectTimer;
    int _reconnectAttempt {0};
    bool _everConnected {false};
    WsLinkOptions _opts;
    Url _url;
    std::exception_ptr _initError;
    std::atomic<bool> _stopping {false};
};

}
