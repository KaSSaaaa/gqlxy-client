#include <gqlxy/links/ws_link.h>

#include <gqlxy/internal/ws/connection/ws_connection_context.h>
#include <boost/uuid/uuid_io.hpp>

using namespace std;
using namespace gqlxy;
using namespace rxcpp;
using namespace boost;

WsLink::WsLink(const WsLinkOptions& options)
    : _options(options),
      _connection(make_shared<internal::WsConnectionContext>(_options)) {}

WsLink::~WsLink() {
    _connection->Stop();
}

Observable<GraphQLResponse> WsLink::Execute(const GraphQLRequest& request) {
    return observable<>::create<GraphQLResponse>([conn = _connection, req = request, this](const auto& s) {
        const auto id = GenerateId();
        s.add([conn, id]() { conn->Unsubscribe(id); });
        conn->Subscribe(id, req, s);
    });
}

string WsLink::GenerateId() {
    lock_guard lock(_uuidMutex);
    return uuids::to_string(_uuidGenerator());
}
