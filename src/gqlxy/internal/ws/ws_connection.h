#pragma once

#include <gqlxy/client/results.h>
#include <gqlxy/internal/url.h>
#include <gqlxy/links/ws_link.h>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <nlohmann/json.hpp>
#include <rxcpp/rx.hpp>

#include <atomic>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <thread>

namespace gqlxy::internal {

struct WsSubscription {
    GraphQLRequest request;
    rxcpp::subscriber<GraphQLResult> subscriber;
};

// Manages a single persistent WebSocket connection for a WsLink instance.
// All internal state is accessed exclusively from the io_context thread.
// Public methods (subscribe/unsubscribe) are thread-safe via asio::post.
class WsConnection : public std::enable_shared_from_this<WsConnection> {
public:
    explicit WsConnection(const WsLinkOptions& opts);
    ~WsConnection();

    void Subscribe(const std::string& id, const GraphQLRequest& req, const rxcpp::subscriber<GraphQLResult>& sub);
    void Unsubscribe(const std::string& id);

private:
    enum class State {
        Idle,
        Connecting,
        Connected,
        Reconnecting
    };

    using PlainWs = boost::beast::websocket::stream<boost::beast::tcp_stream>;
    using TlsWs = boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>;

    WsLinkOptions _opts;
    ParsedUrl _url;
    std::exception_ptr _initError;

    boost::asio::io_context _ioc;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _work;
    std::thread _thread;

    std::unique_ptr<boost::asio::ssl::context> _sslCtx;
    std::unique_ptr<PlainWs> _plainWs;
    std::unique_ptr<TlsWs> _tlsWs;

    State _state {State::Idle};
    std::map<std::string, WsSubscription> _subs;

    boost::beast::flat_buffer _readBuf;
    std::deque<std::string> _writeQueue;
    bool _writing {false};

    int _reconnectAttempt {0};
    bool _everConnected {false};
    boost::asio::steady_timer _reconnectTimer;
    std::atomic<bool> _stopping {false};

    void DoConnect();
    void CreateStream();
    void OnTcpConnected(boost::beast::error_code ec);
    void OnSslHandshake(boost::beast::error_code ec);
    void DoWsHandshake();
    void OnWsHandshake(boost::beast::error_code ec);
    void DoRead();
    void OnRead(boost::beast::error_code ec, std::size_t bytes);
    void Dispatch(const nlohmann::json& msg);
    void SendSubscribe(const std::string& id);
    void EnqueueWrite(const std::string& msg);
    void DoWrite();
    void OnWrite(boost::beast::error_code ec);
    void CompleteSub(const std::string& id);
    void FailAll(std::exception_ptr ex);
    void ScheduleReconnect();
    void ReplaySubscriptions();

    template<typename F>
    auto WithWs(F&& fn) {
        if (_plainWs) return fn(*_plainWs);
        return fn(*_tlsWs);
    }
};

}
