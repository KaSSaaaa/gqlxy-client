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
};

template<typename T>
Result<T> to_result(gqlxy::Observable<T> obs) {
    Result<T> out;
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;

    obs.subscribe(
        [&](const T& v) {
            std::lock_guard lk(mtx);
            out.values.push_back(v);
        },
        [&](std::exception_ptr e) {
            {
                std::lock_guard lk(mtx);
                out.exception = e;
                done = true;
            }
            cv.notify_one();
        },
        [&]() {
            {
                std::lock_guard lk(mtx);
                out.completed = true;
                done = true;
            }
            cv.notify_one();
        });

    std::unique_lock lk(mtx);
    cv.wait(lk, [&] { return done; });
    return out;
}

#define ASSERT_GQL_SUCCESS(out)                                                                                        \
    {                                                                                                                  \
        ASSERT_FALSE((out).exception);                                                                                 \
        ASSERT_FALSE((out).values.empty());                                                                            \
        if ((out).values[0].errors) {                                                                                  \
            FAIL() << "GraphQL errors: " << (out).values[0].errors->front().message;                                   \
        }                                                                                                              \
        ASSERT_TRUE((out).values[0].data.has_value()) << "data field is absent";                                       \
    }
