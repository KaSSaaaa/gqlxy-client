#pragma once

#include <boost/asio/buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <optional>
#include <string>

namespace gqlxy::internal {

inline boost::asio::ssl::context CreateSslContext(const std::optional<std::string>& caCert) {
    boost::asio::ssl::context ctx(boost::asio::ssl::context::tlsv13_client);
    ctx.set_verify_mode(boost::asio::ssl::verify_peer);

    if (caCert) ctx.add_certificate_authority(boost::asio::buffer(*caCert));
    else ctx.set_default_verify_paths();
    return ctx;
}

}
