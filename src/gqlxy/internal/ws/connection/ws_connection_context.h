#pragma once

#include <gqlxy/internal/url.h>
#include <gqlxy/links/ws_link.h>

#include <boost/asio/steady_timer.hpp>
#include <rxcpp/rx.hpp>

#include <atomic>
#include <exception>
#include <map>
#include <memory>
#include <string>

namespace gqlxy::internal {

struct WsSubscription {
    GraphQLRequest request;
    rxcpp::subscriber<GraphQLResult> subscriber;
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

    void Subscribe(const std::string& id, const GraphQLRequest& req, const rxcpp::subscriber<GraphQLResult>& sub);
    void Unsubscribe(const std::string& id);
    void Stop();

private:
    void OnSubscribe(const std::string& id, const GraphQLRequest& req, const rxcpp::subscriber<GraphQLResult>& sub);
    void OnUnsubscribe(const std::string& id);
    void OnTransportConnected();
    void OnTransportMessage(const std::string& raw);
    void OnTransportDisconnected();
    void TransitionTo(ConnectionState state);

    void AddSub(const std::string& id, const GraphQLRequest& req, const rxcpp::subscriber<GraphQLResult>& sub);
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
