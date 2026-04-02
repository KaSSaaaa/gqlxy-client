#pragma once

#include <gqlxy/link.h>
#include <functional>
#include <memory>

namespace gqlxy {

// Routes requests between two links based on a predicate.
// If predicate(request) is true, the left link handles the request; otherwise the right link does.
class SplitLink : public Link {
public:
    using Predicate = std::function<bool(const GraphQLRequest&)>;

    SplitLink(Predicate condition, std::shared_ptr<Link> left, std::shared_ptr<Link> right);

    Observable<GraphQLResult> Execute(const GraphQLRequest& request) override;

private:
    Predicate condition_;
    std::shared_ptr<Link> left_;
    std::shared_ptr<Link> right_;
};

} // namespace gqlxy
