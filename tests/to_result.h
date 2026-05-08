#pragma once

#include <gqlxy/client/observable.h>
#include <gtest/gtest.h>

#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <vector>

template<typename T>
struct Result {
    std::vector<T> values;
    std::exception_ptr exception;
    bool completed = false;
    bool done() const {
        return completed || exception != nullptr;
    }
};

template<typename T>
Result<T> to_result(gqlxy::Observable<T> obs) {
    Result<T> out;
    std::mutex mtx;
    std::condition_variable cv;

    auto locked = [&mtx](const auto& callback) {
        std::lock_guard lock(mtx);
        return callback();
    };

    auto sub = obs.subscribe(
        [&](const T& v) { locked([&]() { out.values.push_back(v); });
        },
        [&](std::exception_ptr e) {
            locked([&]() { out.exception = e; });
            cv.notify_one();
        },
        [&]() {
            locked([&]() { out.completed = true; });
            cv.notify_one();
        });

    std::unique_lock lock(mtx);
    cv.wait_for(lock, std::chrono::seconds(10), [&] { return out.done(); });
    return out;
}

#define ASSERT_GQL_SUCCESS(out)                                                                                        \
    {                                                                                                                  \
        ASSERT_FALSE((out).exception);                                                                                 \
        ASSERT_FALSE((out).values.empty());                                                                            \
        ASSERT_FALSE((out).values[0].errors) << "GraphQL errors: " << (out).values[0].errors->front().message;         \
        ASSERT_TRUE((out).values[0].data.has_value()) << "data field is absent";                                       \
    }
