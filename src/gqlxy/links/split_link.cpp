#include <gqlxy/links/split_link.h>

namespace gqlxy {

SplitLink::SplitLink(Predicate condition, std::shared_ptr<Link> left, std::shared_ptr<Link> right)
    : condition_(std::move(condition)), left_(std::move(left)), right_(std::move(right)) {}

Observable<GraphQLResult> SplitLink::Execute(const GraphQLRequest& request) {
    return condition_(request) ? left_->Execute(request) : right_->Execute(request);
}

} // namespace gqlxy
