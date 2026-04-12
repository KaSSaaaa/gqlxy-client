#pragma once

#include <rxcpp/rx.hpp>

namespace gqlxy {

class Subscription {
public:
    Subscription() = default;
    explicit Subscription(rxcpp::composite_subscription cs) : _cs(std::move(cs)) {}

    void Unsubscribe() { _cs.unsubscribe(); }
    bool IsActive() const { return _cs.is_subscribed(); }

private:
    rxcpp::composite_subscription _cs;
};

}
