#pragma once

#include <atomic>
#include <coroutine>
#include <exception>
#include <memory>
#include <optional>
#include <rxcpp/rx.hpp>
#include <stdexcept>

namespace gqlxy {

// A reactive stream that supports both RxCpp subscription and C++20 co_await.
//
// co_await resolves the first emitted value — suitable for query and mutation:
//   auto result = co_await client.Query("...");
//
// .subscribe() streams all values — suitable for subscriptions:
//   client.Subscribe("...").subscribe([](auto r) { ... });
template <typename T>
class Observable {
public:
    Observable() = default;

    // Accept any rxcpp observable source (typed or dynamic) and erase to dynamic_observable.
    template <typename Source>
    Observable(rxcpp::observable<T, Source> inner) : _inner(inner.as_dynamic()) {}

    template <typename OnNext>
    auto subscribe(OnNext&& on_next) const {
        return _inner.subscribe(std::forward<OnNext>(on_next));
    }

    template <typename OnNext, typename OnError>
    auto subscribe(OnNext&& on_next, OnError&& on_error) const {
        return _inner.subscribe(std::forward<OnNext>(on_next), std::forward<OnError>(on_error));
    }

    template <typename OnNext, typename OnError, typename OnCompleted>
    auto subscribe(OnNext&& on_next, OnError&& on_error, OnCompleted&& on_completed) const {
        return _inner.subscribe(
            std::forward<OnNext>(on_next),
            std::forward<OnError>(on_error),
            std::forward<OnCompleted>(on_completed)
        );
    }

    // Implicit conversion for interop with rxcpp operators (map, filter, merge, etc.)
    operator rxcpp::observable<T>() const { return _inner; }

    // co_await resolves the first emitted value.
    // Throws if the observable completes without emitting or emits an error.
    auto operator co_await() const { return Awaiter{_inner}; }

private:
    rxcpp::observable<T> _inner;

    struct AwaiterState {
        std::optional<T> value;
        std::exception_ptr exception;
        std::atomic<bool> resumed{false};
    };

    struct Awaiter {
        rxcpp::observable<T> observable;
        std::shared_ptr<AwaiterState> state = std::make_shared<AwaiterState>();

        bool await_ready() noexcept { return false; }

        void await_suspend(std::coroutine_handle<> handle) {
            observable.take(1).subscribe(
                [s = state, h = handle](T v) {
                    s->value = std::move(v);
                    if (!s->resumed.exchange(true)) h.resume();
                },
                [s = state, h = handle](std::exception_ptr e) {
                    s->exception = e;
                    if (!s->resumed.exchange(true)) h.resume();
                },
                [s = state, h = handle]() {
                    if (!s->value && !s->resumed.exchange(true)) {
                        s->exception = std::make_exception_ptr(
                            std::runtime_error("Observable completed without emitting a value"));
                        h.resume();
                    }
                }
            );
        }

        T await_resume() {
            if (state->exception) std::rethrow_exception(state->exception);
            return std::move(*state->value);
        }
    };
};

}
