---
title: Subscriptions
description: How to execute subscriptions with gqlxy-client
---

## Using Subscriptions for reactive data

Subscriptions open a persistent event stream to the server. Unlike queries and mutations, a subscription emits multiple values over time and must be consumed reactively via `.subscribe()`.

`Subscribe()` returns a `Subscription` object, keeping the GraphQL subscription alive. To unsubscribe, simply call
`.Unsubscribe()`. When the `Subscription` is deleted, it is also unsubscribed.

```cpp
Subscription sub = client.Subscribe({
    .query = R"(
        subscription OnMessageAdded {
            messageAdded {
                id
                text
                author
            }
        }
    )"
}).subscribe(
    [](const GraphQLResponse& event) {
        std::cout << event.data->dump(2) << "\n";
    },
    [](std::exception_ptr) { /* handle stream error */ }
);

// Later: cancel the subscription
sub.Unsubscribe();
```

## Choosing a transport

To enable reactive data from subscriptions, it is required to have a stateful connection like WebSocket or Server-Sent Events (SSE).
In gqlxy-client, you can use one or both `HttpLink` or `WsLink`. By default, `HttpLink` will use SSE for subscriptions.

If you want, you can also use both with a `SplitLink` to route subscription operations to WebSockets.

```cpp
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/ws_link.h>
#include <gqlxy/links/split_link.h>

auto client = gqlxy::Client({
    .link = make_shared<SplitLink>(
        [](const GraphQLRequest& req) {
            // When true, the request will use the first link.
            // Otherwise, it will use the second one.
            return req.type != parser::OperationType::SUBSCRIPTION;
        },
        make_shared<HttpLink>(HttpLinkOptions{
            .url = "http://localhost:4000/graphql"
        }),
        make_shared<WsLink>(WsLinkOptions{
            .url = "ws://localhost:4000/graphql"
        })
    ),
    .cache = make_shared<InMemoryCache>()
});
```

## Handling completion

When using `.subscribe`, you can also have a third callback to handle completion.

```cpp
Subscription sub = client.Subscribe({.query = "subscription { heartbeat }"})
    .subscribe(
        [](const GraphQLResponse& r) { /* event */ },
        [](std::exception_ptr) { /* error */ },
        []() { /* done */ }
    );
```

## Cancelling a subscription

To cancel a subscription, simply call `.Unsubscribe()` on the `Subscription` object. Otherwise, the subscription will
automatically be canceled when the object gets deleted.

```cpp
sub.Unsubscribe();
```

## Subscription variables

Like every other request, you can pass variables to your GraphQL subscriptions.

```cpp
Subscription sub = client.Subscribe({
    .query = R"(
        subscription OnReview($bookId: ID!) {
            reviewAdded(bookId: $bookId) {
                rating
            }
        }
    )",
    .variables = {
        {"bookId", "42"}
    }
}).subscribe(...);
```

## Using RPP operators

`Observable<GraphQLResponse>` converts implicitly to `rpp::dynamic_observable<GraphQLResponse>`, giving access to the full RPP operator set:

```cpp
rpp::dynamic_observable<GraphQLResponse> stream = client.Subscribe({
    .query = R"(
        subscription {
            priceUpdated {
                symbol
                bid
                ask
            }
        }
    )"
});

Subscription sub = (stream
    | rpp::operators::filter([](const GraphQLResponse& r) { return r.data.has_value(); })
    | rpp::operators::map([](const GraphQLResponse& r) { return (*r.data)["priceUpdated"]; }))
    .subscribe([](const nlohmann::json& tick) {
        std::cout << format("{} {}/{}", tick["symbol"].get<std::string>(), tick["bid"], tick["ask"]) << std::endl;
    });
```

## Error handling

Transport errors (e.g. WebSocket disconnect) are delivered through the `on_error` callback and terminate the observable.
`WsLink` performs automatic reconnection at the transport level. A brief disconnect will be recovered transparently without
surfacing an error to the subscription unless the retry budget is exhausted.

GraphQL field errors are delivered as a normal `GraphQLResponse` with the `.errors` field. The stream remains open and continues
delivering subsequent events.
