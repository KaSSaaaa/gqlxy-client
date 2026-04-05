#include <gqlxy/links/ws_link.h>

#include <gqlxy/internal/ws/ws_connection.h>

#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

using namespace std;
using namespace gqlxy;
using namespace rxcpp;

WsLink::WsLink(const WsLinkOptions& options)
    : _options(std::move(options)),
      _connection(make_shared<internal::WsConnection>(_options)) {}

Observable<GraphQLResult> WsLink::Execute(const GraphQLRequest& request) {
    return observable<>::create<GraphQLResult>([conn = _connection, req = request](const auto& s) {
        const auto id = boost::uuids::to_string(boost::uuids::random_generator()());
        s.add([conn, id]() { conn->Unsubscribe(id); });
        conn->Subscribe(id, req, s);
    });
}
