#pragma once

#include <gqlxy/results.h>
#include <gqlxy/server/standalone_server.h>
#include <gtest/gtest.h>

#include "server/schema.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>

namespace gqlxy::e2e {

static const std::string ServerUrl = "http://localhost:4001/graphql";
static constexpr uint16_t Port = 4001;

inline bool wait_for_port(uint16_t port, std::chrono::seconds timeout = std::chrono::seconds {15}) {
    namespace asio = boost::asio;
    using tcp = asio::ip::tcp;

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        try {
            asio::io_context ioc;
            tcp::socket sock(ioc);
            boost::system::error_code ec;
            sock.connect(tcp::endpoint(asio::ip::address_v4::loopback(), port), ec);
            if (!ec) return true;
        } catch (...) {
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

class ServerEnvironment : public testing::Environment {
    std::unique_ptr<server::StandaloneServer> _server;

public:
    void SetUp() override {
        auto schema = MakeE2ESchema();
        _server = std::make_unique<server::StandaloneServer>(
            server::StandaloneServerOptions {.schema = schema, .port = Port});
        _server->StartAsync();
        if (!wait_for_port(Port)) throw std::runtime_error("E2E server did not become ready within timeout");
    }

    void TearDown() override {
        if (_server) {
            _server->Stop();
            _server.reset();
        }
    }
};

}
