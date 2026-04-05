#include <gqlxy/client.h>

using namespace std;
using namespace gqlxy;
using namespace nlohmann;

Client::Client(const ClientOptions& options) : _options(options) {}

Observable<GraphQLResult> Client::Query(const string& query, const json& variables) {
    return Execute({.query = query, .variables = variables, .type = OperationType::Query});
}

Observable<GraphQLResult> Client::Mutation(const string& query, const json& variables) {
    return Execute({.query = query, .variables = variables, .type = OperationType::Mutation});
}

Observable<GraphQLResult> Client::Subscribe(const string& query, const json& variables) {
    return Execute({.query = query, .variables = variables, .type = OperationType::Subscription});
}

Observable<GraphQLResult> Client::Execute(const GraphQLRequest& request) {
    return _options.link->Execute(request);
}
