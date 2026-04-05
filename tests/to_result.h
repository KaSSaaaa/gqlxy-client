#pragma once

#include <gqlxy/observable.h>
#include <gtest/gtest.h>

#include <exception>
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
    obs.subscribe(
        [&](T v) { out.values.push_back(std::move(v)); }, [&](std::exception_ptr e) { out.exception = e; },
        [&]() { out.completed = true; });
    return out;
}

// Asserts a Collected<GraphQLResult> has no exception, at least one value, and no GraphQL errors.
#define ASSERT_GQL_SUCCESS(out)                                                                                        \
    {                                                                                                                  \
        ASSERT_FALSE((out).exception);                                                                                 \
        ASSERT_FALSE((out).values.empty());                                                                            \
        if ((out).values[0].errors) {                                                                                  \
            FAIL() << "GraphQL errors: " << (out).values[0].errors->front().message;                                   \
        }                                                                                                              \
        ASSERT_TRUE((out).values[0].data.has_value()) << "data field is absent";                                       \
    }
