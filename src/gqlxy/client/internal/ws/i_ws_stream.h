#pragma once

#include "i_ws_stream.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/websocket/stream_base.hpp>

namespace gqlxy::internal {

using ConnectCallback = std::function<void(const boost::beast::error_code&)>;
using IOCallback = std::function<void(boost::beast::error_code, std::size_t)>;

class IWsStream {
public:
    virtual ~IWsStream() = default;

    virtual void Connect(
        const boost::asio::ip::tcp::resolver::results_type& endpoints, const std::chrono::seconds& timeout,
        const ConnectCallback& callback) = 0;
    virtual void Read(boost::beast::flat_buffer& buf, const IOCallback& callback) = 0;
    virtual void Write(const boost::asio::const_buffer& buffer, const IOCallback& callback) = 0;

    virtual void Close() = 0;
};

}
