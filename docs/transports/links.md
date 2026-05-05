---
title: Links
description: How GQLXY Client transports GraphQL operations over HTTP, WebSocket, and SSE.
---

A **Link** is the transport layer for `Client`. Every `Client` is constructed with a single root link; that link is responsible for executing each `GraphQLRequest` and returning an `Observable<GraphQLResponse>`.

Links are composable: `SplitLink` routes between two links based on a predicate, letting you send queries and mutations over HTTP while streaming subscriptions over WebSocket.

## HttpLink

**Header:** `<gqlxy/links/http_link.h>`

`HttpLink` executes queries and mutations as HTTP `POST` requests to a GraphQL endpoint using Boost.Beast. It automatically detects subscription operations and switches to Server-Sent Events (`text/event-stream`) when the server advertises SSE support.

```cpp
#include <gqlxy/links/http_link.h>

using namespace gqlxy;

auto link = std::make_shared<HttpLink>(HttpLinkOptions{
    .url = "http://localhost:4000/graphql"
});
```

### HttpLinkOptions

```cpp
struct LinkOptions {
    std::string url;
    Headers headers;                      // Additional request headers
    std::optional<std::string> caCert;    // PEM-encoded CA certificate for TLS
};

using HttpLinkOptions = LinkOptions;
```

| Field     | Description                                                                               |
|-----------|-------------------------------------------------------------------------------------------|
| `url`     | Full HTTP or HTTPS endpoint URL                                                           |
| `headers` | `std::map<std::string, std::string>` of additional request headers (e.g. `Authorization`) |
| `caCert`  | PEM string for self-signed certificate verification. Omit to use the system trust store.  |

### TLS (HTTPS)

Switch the URL scheme to `https://`. No other changes are required:

```cpp
auto link = std::make_shared<HttpLink>(HttpLinkOptions{
    .url = "https://api.example.com/graphql"
});
```

For self-signed certificates (e.g. local development):

```cpp
auto link = std::make_shared<HttpLink>(HttpLinkOptions{
    .url    = "https://localhost:4000/graphql",
    .caCert = R"(-----BEGIN CERTIFICATE-----
...
-----END CERTIFICATE-----)"
});
```

### Custom headers

```cpp
auto link = std::make_shared<HttpLink>(HttpLinkOptions{
    .url     = "https://api.example.com/graphql",
    .headers = {
        {"Authorization", "Bearer " + token},
        {"X-Client-Version", "1.0"}
    }
});
```

### SSE subscriptions

When `HttpLink` receives a response with `Content-Type: text/event-stream`, it automatically switches into SSE streaming mode. Subscription events are parsed from the SSE `data:` lines and emitted through the same `Observable<GraphQLResponse>` — no configuration needed.

```cpp
// This subscription is automatically streamed via SSE if the server supports it
Subscription sub = client.Subscribe({
    .query = "subscription { messageAdded { text } }"
}).subscribe([](const GraphQLResponse& event) {
    std::cout << event.data->dump() << "\n";
});
```

---

## WsLink

**Header:** `<gqlxy/links/ws_link.h>`

`WsLink` maintains a **single persistent WebSocket connection** (per `WsLink` instance) using the [`graphql-transport-ws`](https://github.com/enisdenjo/graphql-ws/blob/master/PROTOCOL.md) protocol. All operations — queries, mutations, and subscriptions — are multiplexed over that connection.

```cpp
#include <gqlxy/links/ws_link.h>

using namespace gqlxy;

auto link = std::make_shared<WsLink>(WsLinkOptions{
    .url = "ws://localhost:4000/graphql"
});
```

### WsLinkOptions

```cpp
using WsLinkOptions = LinkOptions;
```

Same fields as `HttpLinkOptions`: `url`, `headers`, and `caCert`. Use `ws://` or `wss://`.

### Persistent connection

The WebSocket connection is opened on the first `Execute()` call and reused for all subsequent operations. If the connection drops, `WsLink` automatically reconnects with exponential back-off (capped at 30 seconds) and replays all active subscriptions on the new connection.

### WSS (WebSocket over TLS)

```cpp
auto link = std::make_shared<WsLink>(WsLinkOptions{
    .url = "wss://api.example.com/graphql"
});
```

### Lifecycle

The connection is held open as long as the `WsLink` object lives. When it is destroyed, the connection is closed cleanly and all active subscribers receive `on_completed`.

---

## SplitLink

**Header:** `<gqlxy/links/split_link.h>`

`SplitLink` routes each operation to one of two child links based on a predicate. The canonical usage is sending queries and mutations to `HttpLink` and subscriptions to `WsLink`.

```cpp
#include <gqlxy/links/split_link.h>

using namespace gqlxy;

auto link = std::make_shared<SplitLink>(
    [](const GraphQLRequest& req) {
        return req.type != parser::OperationType::SUBSCRIPTION;
    },
    std::make_shared<HttpLink>(HttpLinkOptions{.url = "http://localhost:4000/graphql"}),
    std::make_shared<WsLink>(WsLinkOptions{.url  = "ws://localhost:4000/graphql"})
);
```

The predicate receives the `GraphQLRequest` **after** all document transforms have been applied (including `AddTypename`). When it returns `true`, the first link handles the request; when it returns `false`, the second link handles it.

### SplitLink constructor

```cpp
SplitLink(
    std::function<bool(const GraphQLRequest&)> predicate,
    std::shared_ptr<Link> leftLink,   // used when predicate returns true
    std::shared_ptr<Link> rightLink   // used when predicate returns false
);
```

---

## Writing a custom Link

Any class that inherits from `Link` and implements `Execute()` is a valid link:

```cpp
#include <gqlxy/link.h>

class LoggingLink : public gqlxy::Link {
public:
    LoggingLink(std::shared_ptr<gqlxy::Link> inner) : _inner(std::move(inner)) {}

    gqlxy::Observable<gqlxy::GraphQLResponse> Execute(const gqlxy::GraphQLRequest& request) override {
        std::cout << "→ " << request.query << "\n";
        return static_cast<rxcpp::observable<gqlxy::GraphQLResponse>>(_inner->Execute(request))
            .tap([](const gqlxy::GraphQLResponse& r) {
                std::cout << "← " << (r.data ? r.data->dump() : "<no data>") << "\n";
            });
    }

private:
    std::shared_ptr<gqlxy::Link> _inner;
};
```
