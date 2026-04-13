#pragma once

#include "server/schema.h"
#include "test_certs.h"
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <gqlxy/server/standalone_server.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <thread>

namespace gqlxy::e2e {

static const std::string ServerUrl = "http://localhost:4001/graphql";
static const std::string WsServerUrl = "ws://localhost:4001/graphql";
static const std::string HttpsServerUrl = "https://localhost:4002/graphql";
static const std::string WssServerUrl = "wss://localhost:4002/graphql";
static constexpr uint16_t Port = 4001;
static constexpr uint16_t TlsPort = 4002;

inline bool WaitForPort(uint16_t port, std::chrono::seconds timeout = std::chrono::seconds {15}) {
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
    std::optional<Schema> _schema;
    std::unique_ptr<server::StandaloneServer> _server;
    std::unique_ptr<server::StandaloneServer> _tlsServer;
    std::filesystem::path _certPath;
    std::filesystem::path _keyPath;

    void WriteCertFiles() {
        const auto tmp = std::filesystem::temp_directory_path();
        _certPath = tmp / "gqlxy_e2e_server.cert.pem";
        _keyPath = tmp / "gqlxy_e2e_server.key.pem";
        std::ofstream(_certPath) << ServerCert;
        std::ofstream(_keyPath) << ServerKey;
    }

public:
    void SetUp() override {
        WriteCertFiles();

        _schema.emplace(CreateSchema());

        _server = std::make_unique<server::StandaloneServer>(server::StandaloneServerOptions {
            .schema = *_schema,
            .port = Port
        });
        _server->StartAsync();
        if (!WaitForPort(Port)) throw std::runtime_error("E2E server did not become ready within timeout");

        _tlsServer = std::make_unique<server::StandaloneServer>(server::StandaloneServerOptions {
            .schema = *_schema,
            .port = TlsPort,
            .tls = server::TlsOptions {
                .certPath = _certPath.string(),
                .keyPath = _keyPath.string()
            }
        });
        _tlsServer->StartAsync();
        if (!WaitForPort(TlsPort)) throw std::runtime_error("TLS server did not become ready within timeout");
    }

    void TearDown() override {
        if (_tlsServer) {
            _tlsServer->Stop();
            _tlsServer.reset();
        }
        if (_server) {
            _server->Stop();
            _server.reset();
        }
        _schema.reset();
        std::filesystem::remove(_certPath);
        std::filesystem::remove(_keyPath);
    }
};

}
