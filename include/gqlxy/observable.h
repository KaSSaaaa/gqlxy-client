#pragma once

#include <atomic>
#include <coroutine>
#include <exception>
#include <memory>
#include <optional>
#include <rxcpp/rx.hpp>
#include <stdexcept>

namespace gqlxy {

template<typename T>
class Observable {
public:
    Observable() = default;

    template<typename Source>
    Observable(rxcpp::observable<T, Source> inner) : _inner(inner.as_dynamic()) {}

    template<typename OnNext>
    auto subscribe(OnNext&& on_next) const {
        return _inner.subscribe(std::forward<OnNext>(on_next));
    }

    template<typename OnNext, typename OnError>
    auto subscribe(OnNext&& on_next, OnError&& on_error) const {
        return _inner.subscribe(std::forward<OnNext>(on_next), std::forward<OnError>(on_error));
    }

    template<typename OnNext, typename OnError, typename OnCompleted>
    auto subscribe(OnNext&& on_next, OnError&& on_error, OnCompleted&& on_completed) const {
        return _inner.subscribe(
            std::forward<OnNext>(on_next), std::forward<OnError>(on_error), std::forward<OnCompleted>(on_completed));
    }

    operator rxcpp::observable<T>() const {
        return _inner;
    }

    auto operator co_await() const {
        return Awaiter {_inner};
    }

private:
    rxcpp::observable<T> _inner;

    struct AwaiterState {
        std::optional<T> value;
        std::exception_ptr exception;
        std::atomic<bool> resumed {false};
    };

    struct Awaiter {
        rxcpp::observable<T> observable;
        std::shared_ptr<AwaiterState> state = std::make_shared<AwaiterState>();

        bool await_ready() noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) {
            observable.take(1).subscribe(
                [s = state, h = handle](const T& v) {
                    s->value = v;
                    if (!s->resumed.exchange(true)) h.resume();
                },
                [s = state, h = handle](const std::exception_ptr& e) {
                    s->exception = e;
                    if (!s->resumed.exchange(true)) h.resume();
                },
                [s = state, h = handle]() {
                    if (!s->value && !s->resumed.exchange(true)) {
                        s->exception = std::make_exception_ptr(
                            std::runtime_error("Observable completed without emitting a value"));
                        h.resume();
                    }
                });
        }

        T await_resume() {
            if (state->exception) std::rethrow_exception(state->exception);
            return std::move(*state->value);
        }
    };
};

}
