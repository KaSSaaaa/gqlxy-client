#pragma once

#include <functional>
#include <gqlxy/client/link.h>
#include <memory>

namespace gqlxy {

class SplitLink : public Link {
public:
    SplitLink(
        const std::function<bool(const GraphQLRequest&)>& condition, const std::shared_ptr<Link>& left,
        const std::shared_ptr<Link>& right);

    Observable<GraphQLResponse> Execute(const GraphQLRequest& request) override;

private:
    std::function<bool(const GraphQLRequest&)> _condition;
    std::shared_ptr<Link> _left;
    std::shared_ptr<Link> _right;
};

}
