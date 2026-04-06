#pragma once

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <thread>

namespace gqlxy::internal {

// Singleton io_context running on a background thread.
// Shared by all link implementations that need async I/O.
class AsioContext {
public:
    static boost::asio::io_context& Get() {
        static AsioContext instance;
        return instance._context;
    }

    static bool OnContext() { return _onContextThread; }

    AsioContext(const AsioContext&) = delete;
    AsioContext& operator=(const AsioContext&) = delete;

private:
    inline static thread_local bool _onContextThread = false;

    AsioContext()
        : _lock(boost::asio::make_work_guard(_context)),
          _contextThread([this] {
              _onContextThread = true;
              _context.run();
          })
    {}

    ~AsioContext() {
        _lock.reset();
        if (_contextThread.joinable()) _contextThread.join();
    }

    boost::asio::io_context _context;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> _lock;
    std::thread _contextThread;
};

}
