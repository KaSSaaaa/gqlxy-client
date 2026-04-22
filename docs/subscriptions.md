---
title: Subscriptions
---

Subscriptions open a persistent event stream to the server. Unlike queries and mutations, a subscription emits multiple values over time and must be consumed reactively via `.subscribe()`.

## Choosing a transport

Subscriptions require a stateful connection. Use `WsLink` or `SseLink` directly, or use `SplitLink` to route subscription operations away from `HttpLink`:

```cpp
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/ws_link.h>
#include <gqlxy/links/split_link.h>

auto client = gqlxy::Client({
    .link = make_shared<SplitLink>(
        [](const GraphQLRequest& req) {
            return req.type != parser::OperationType::Subscription;
        },
        make_shared<HttpLink>(HttpLinkOptions{.url = "http://localhost:4000/graphql"}),
        make_shared<WsLink>(WsLinkOptions{.url  = "ws://localhost:4000/graphql"})
    ),
    .cache = make_shared<InMemoryCache>()
});
```

`HttpLink` also supports subscriptions over SSE when the server responds with `Content-Type: text/event-stream`, so a `SplitLink` is not strictly required if your server uses that protocol.

## Opening a subscription

Call `Client::Subscribe()` with a `SubscribeOptions` struct, then attach handlers with `.subscribe()`. The returned `Subscription` is a RAII handle — keep it alive for as long as you want to receive events.

```cpp
#include <gqlxy/client.h>

Subscription sub = client.Subscribe({
    .query = R"(
        subscription OnMessage {
            messageAdded {
                id
                text
                author
            }
        }
    )"
}).subscribe(
    [](const GraphQLResponse& response) {
        if (response.data)
            std::cout << response.data->dump(2) << std::endl;
    },
    [](std::exception_ptr e) {
        try { std::rethrow_exception(e); }
        catch (const std::exception& ex) { std::cerr << ex.what() << std::endl; }
    }
);
```

## Handling completion

Pass a third callback to be notified when the stream ends:

```cpp
Subscription sub = client.Subscribe({.query = "subscription { heartbeat }"})
    .subscribe(
        [](const GraphQLResponse& r) { /* event */ },
        [](std::exception_ptr)       { /* error  */ },
        []()                         { /* done   */ }
    );
```

## Cancelling a subscription

Call `.Unsubscribe()` on the handle to stop receiving events and release the underlying connection slot:

```cpp
sub.Unsubscribe();
```

The handle also cancels automatically when it goes out of scope.

## Checking subscription state

```cpp
if (sub.IsActive()) {
    // still receiving events
}
```

## Subscription variables

```cpp
Subscription sub = client.Subscribe({
    .query     = R"( subscription OnReview($bookId: ID!) { reviewAdded(bookId: $bookId) { rating } } )",
    .variables = {{"bookId", "42"}}
}).subscribe(...);
```

## Using rxcpp operators

`Observable<GraphQLResponse>` converts implicitly to `rxcpp::observable<GraphQLResponse>`, giving access to the full rxcpp operator set:

```cpp
rxcpp::observable<GraphQLResponse> stream = client.Subscribe({
    .query = R"( subscription { priceUpdated { symbol bid ask } } )"
});

Subscription sub = stream
    .filter([](const GraphQLResponse& r) { return r.data.has_value(); })
    .map([](const GraphQLResponse& r) { return (*r.data)["priceUpdated"]; })
    .subscribe([](const nlohmann::json& tick) {
        std::cout << tick["symbol"].get<std::string>()
                  << " " << tick["bid"] << "/" << tick["ask"] << std::endl;
    });
```

## Error handling

A transport error (e.g. WebSocket disconnect) is delivered to the `on_error` callback and terminates the observable. `WsLink` performs automatic reconnection at the transport level — a brief disconnect will be recovered transparently without surfacing an error to the subscription unless the retry budget is exhausted.

GraphQL field errors within an event are delivered as a normal `GraphQLResponse` with `.errors` populated; the stream remains open and continues delivering subsequent events.
