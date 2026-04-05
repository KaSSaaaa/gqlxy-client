#pragma once

#include <gqlxy/link.h>
#include <functional>
#include <memory>

namespace gqlxy {

class SplitLink : public Link {
public:
    using Predicate = std::function<bool(const GraphQLRequest&)>;

    SplitLink(const Predicate& condition, const std::shared_ptr<Link>& left, const std::shared_ptr<Link>& right);

    Observable<GraphQLResult> Execute(const GraphQLRequest& request) override;

private:
    Predicate _condition;
    std::shared_ptr<Link> _left;
    std::shared_ptr<Link> _right;
};

}
