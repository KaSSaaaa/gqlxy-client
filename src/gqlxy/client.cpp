#include <gqlxy/client.h>

namespace gqlxy {

Client::Client(ClientOptions options) : options_(std::move(options)) {}

Observable<GraphQLResult> Client::Query(std::string_view query, nlohmann::json variables) {
    return Execute({.query = std::string(query), .variables = std::move(variables)});
}

Observable<GraphQLResult> Client::Mutation(std::string_view query, nlohmann::json variables) {
    return Execute({.query = std::string(query), .variables = std::move(variables)});
}

Observable<GraphQLResult> Client::Subscribe(std::string_view query, nlohmann::json variables) {
    return Execute({.query = std::string(query), .variables = std::move(variables)});
}

Observable<GraphQLResult> Client::Execute(GraphQLRequest request) {
    return options_.link->Execute(request);
}

} // namespace gqlxy
