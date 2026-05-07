#include <gqlxy/links/split_link.h>

using namespace std;
using namespace gqlxy;

SplitLink::SplitLink(
    const function<bool(const GraphQLRequest&)>& condition, const shared_ptr<Link>& left, const shared_ptr<Link>& right)
    : _condition(condition),
      _left(left),
      _right(right) {}

Observable<GraphQLResponse> SplitLink::Execute(const GraphQLRequest& request) {
    return _condition(request) ? _left->Execute(request) : _right->Execute(request);
}
