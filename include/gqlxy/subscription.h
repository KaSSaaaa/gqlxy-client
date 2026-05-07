#pragma once

#include <rpp/disposables/composite_disposable.hpp>

namespace gqlxy {

class Subscription {
public:
    Subscription() = default;
    explicit Subscription(rpp::composite_disposable_wrapper disposable) : _disposable(std::move(disposable)) {}

    void Unsubscribe() { _disposable.dispose(); }
    bool IsActive() const { return !_disposable.is_disposed(); }

private:
    rpp::composite_disposable_wrapper _disposable;
};

}
