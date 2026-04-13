#pragma once

#include <gqlxy/observable.h>
#include <gtest/gtest.h>

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

    obs.subscribe(
        [&](const T& v) { out.values.push_back(v); },
        [&](std::exception_ptr e) {
            out.exception = e;
            cv.notify_one();
        },
        [&]() {
            out.completed = true;
            cv.notify_one();
        });

    std::unique_lock lk(mtx);
    cv.wait(lk, [&] { return out.done(); });
    return out;
}

#define ASSERT_GQL_SUCCESS(out)                                                                                        \
    {                                                                                                                  \
        ASSERT_FALSE((out).exception);                                                                                 \
        ASSERT_FALSE((out).values.empty());                                                                            \
        ASSERT_FALSE((out).values[0].errors) << "GraphQL errors: " << (out).values[0].errors->front().message;         \
        ASSERT_TRUE((out).values[0].data.has_value()) << "data field is absent";                                       \
    }
