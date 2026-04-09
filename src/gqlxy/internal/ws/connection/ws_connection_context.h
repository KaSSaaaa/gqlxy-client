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

class IConnectionState;
class WsTransport;

class WsConnectionContext : public std::enable_shared_from_this<WsConnectionContext> {
public:
    explicit WsConnectionContext(const WsLinkOptions& opts);
    ~WsConnectionContext();

    void AddSub(const std::string& id, const GraphQLRequest& req, const rxcpp::subscriber<GraphQLResult>& sub);
    void RemoveSub(const std::string& id);
    bool HasSubs() const;
    void ReplaySubs();
    void FailAll(const std::exception_ptr& ex);

    void Connect();
    void Send(const nlohmann::json& msg);
    void SendSubscribe(const std::string& id, const GraphQLRequest& req);
    void ResetTransport();
    void Dispatch(const std::string& raw);

    void SetConnected();
    bool IsEverConnected() const;
    std::string GetUrl() const;

    void ScheduleReconnect();
    void CancelReconnect();

    void SetState(std::unique_ptr<IConnectionState> next);

    void DispatchSubscribe(const std::string& id, const GraphQLRequest& req, const rxcpp::subscriber<GraphQLResult>& sub);
    void DispatchUnsubscribe(const std::string& id);

    void Stop();
    bool IsStopping() const;
    std::exception_ptr InitError() const;

private:
    void CompleteSub(const std::string& id);

    std::map<std::string, WsSubscription> _subs;
    std::shared_ptr<WsTransport> _transport;
    std::unique_ptr<IConnectionState> _state;
    boost::asio::steady_timer _reconnectTimer;
    int _reconnectAttempt {0};
    bool _everConnected {false};
    WsLinkOptions _opts;
    ParsedUrl _url;
    std::exception_ptr _initError;
    std::atomic<bool> _stopping {false};
};

}
