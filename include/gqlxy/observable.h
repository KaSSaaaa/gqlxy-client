#pragma once

#include <gqlxy/subscription.h>
#include <rpp/observables/dynamic_observable.hpp>
#include <rpp/operators/take.hpp>
#include <atomic>
#include <coroutine>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>

namespace gqlxy {

template<typename T>
class Observable {
public:
    Observable() = default;

    template<typename Source>
    Observable(rpp::observable<T, Source> inner) : _inner(inner.as_dynamic()) {}

    template<typename OnNext>
    Subscription subscribe(OnNext&& onNext) const {
        return subscribe(onNext, [](const auto&){});
    }

    template<typename OnNext, typename OnError>
    Subscription subscribe(OnNext&& onNext, OnError&& onError) const {
        return subscribe(onNext, onError, [](){});
    }

    template<typename OnNext, typename OnError, typename OnCompleted>
    Subscription subscribe(OnNext&& on_next, OnError&& on_error, OnCompleted&& on_completed) const {
        auto disposable = rpp::composite_disposable_wrapper::make();
        _inner.subscribe(disposable, std::forward<OnNext>(on_next), std::forward<OnError>(on_error), std::forward<OnCompleted>(on_completed));
        return Subscription {disposable};
    }

    operator rpp::dynamic_observable<T>() const {
        return _inner;
    }

    auto operator co_await() const {
        return Awaiter {_inner};
    }

private:
    rpp::dynamic_observable<T> _inner;

    struct AwaiterState {
        std::optional<T> value;
        std::exception_ptr exception;
        std::atomic<bool> resumed {false};
    };

    struct Awaiter {
        rpp::dynamic_observable<T> observable;
        std::shared_ptr<AwaiterState> state = std::make_shared<AwaiterState>();

        bool await_ready() noexcept {
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) {
            (observable | rpp::operators::take(1)).subscribe(
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
