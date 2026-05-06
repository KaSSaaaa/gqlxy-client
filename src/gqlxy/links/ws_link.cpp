#include <boost/uuid/uuid_io.hpp>
#include <gqlxy/internal/ws/connection/ws_connection_context.h>
#include <gqlxy/links/ws_link.h>
#include <gqlxy/results.h>
#include <rpp/sources/create.hpp>

using namespace std;
using namespace gqlxy;
using namespace rpp;
using namespace rpp::source;
using namespace boost;

WsLink::WsLink(const WsLinkOptions& options)
    : _options(options),
      _connection(make_shared<internal::WsConnectionContext>(_options)) {}

WsLink::~WsLink() {
    _connection->Stop();
}

Observable<GraphQLResponse> WsLink::Execute(const GraphQLRequest& request) {
    return create<GraphQLResponse>([conn = _connection, req = request, this](dynamic_observer<GraphQLResponse>&& sub) {
        const auto id = GenerateId();
        sub.set_upstream(make_callback_disposable([conn, id]() noexcept {
            conn->Unsubscribe(id);
        }));
        conn->Subscribe(id, req, std::move(sub));
    });
}

string WsLink::GenerateId() {
    lock_guard lock(_uuidMutex);
    return uuids::to_string(_uuidGenerator());
}
