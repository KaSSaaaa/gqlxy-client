---
title: API Reference
---

## `gqlxy::Client`

Top-level entry point. Constructed with a `ClientOptions` struct.

```cpp
#include <gqlxy/client.h>

struct ClientOptions {
    std::shared_ptr<Link> link;
    std::shared_ptr<Cache> cache;
    FetchPolicy defaultFetchPolicy = FetchPolicy::CacheFirst;
    std::vector<DocumentTransform> documentTransforms = { AddTypename };
};

Client(const ClientOptions& options);
```

### Methods

| Method                        | Returns                       | Description                                               |
|-------------------------------|-------------------------------|-----------------------------------------------------------|
| `Query(QueryOptions)`         | `Observable<GraphQLResponse>` | Execute a query; respects fetch policy and cache          |
| `Mutation(MutationOptions)`   | `Observable<GraphQLResponse>` | Execute a mutation; always goes to the network            |
| `Subscribe(SubscribeOptions)` | `Observable<GraphQLResponse>` | Open a subscription event stream                          |
| `Refetch(QueryOptions)`       | `Observable<GraphQLResponse>` | Re-execute a query with `NetworkOnly`, updating the cache |

All methods return `Observable<GraphQLResponse>`. Use `co_await` to resolve the first value inside a coroutine, or call `.subscribe()` to attach reactive handlers.

### Option structs

```cpp
struct QueryOptions {
    std::string query;
    nlohmann::json variables = nullptr;
    std::optional<FetchPolicy> fetchPolicy;  // overrides ClientOptions::defaultFetchPolicy
};

struct MutationOptions {
    std::string query;
    nlohmann::json variables = nullptr;
};

struct SubscribeOptions {
    std::string query;
    nlohmann::json variables = nullptr;
};
```

---

## `gqlxy::Observable<T>`

Thin wrapper around `rpp::dynamic_observable<T>`.

```cpp
template<typename T>
class Observable {
public:
    // Attach a reactive subscriber; returns a Subscription RAII handle
    Subscription subscribe(OnNext) const;
    Subscription subscribe(OnNext, OnError) const;
    Subscription subscribe(OnNext, OnError, OnCompleted) const;

    // Resolves the first emitted value inside a C++20 coroutine
    auto operator co_await() const;

    // Implicit conversion to rpp::dynamic_observable<T> — allows using the full RPP operator set
    operator rpp::dynamic_observable<T>() const;
};
```

`co_await` on an `Observable` that completes without emitting a value throws `std::runtime_error`.

---

## `gqlxy::Subscription`

RAII handle returned by `Observable::subscribe()`.

```cpp
class Subscription {
public:
    void Unsubscribe();
    bool IsActive() const;
};
```

The subscription is cancelled when the handle is destroyed.

---

## `gqlxy::GraphQLResponse`

```cpp
struct GraphQLResponse {
    std::optional<nlohmann::json> data;
    std::optional<std::vector<GraphQLError>> errors;
};
```

---

## `gqlxy::GraphQLRequest`

Internal request type passed through the link chain.

```cpp
struct GraphQLRequest {
    std::string query;
    nlohmann::json variables = nullptr;
    std::optional<std::string> operationName;
    parser::OperationType type = parser::OperationType::QUERY;
    FetchPolicy policy;
};
```

`operationName` is extracted automatically from the query document by `Client`. Do not set it manually.

---

## `gqlxy::FetchPolicy`

```cpp
enum class FetchPolicy {
    CacheFirst,       // Read cache; skip network if hit (default)
    NetworkOnly,      // Always fetch; write to cache
    CacheAndNetwork,  // Emit cache first, then re-fetch and emit network result
    NoCache           // Always fetch; do not read or write cache
};
```

---

## `gqlxy::Link`

Abstract base for all transport adapters.

```cpp
class Link {
public:
    virtual Observable<GraphQLResponse> Execute(const GraphQLRequest& request) = 0;
};
```

### `LinkOptions`

Shared configuration struct used by `HttpLink`, `WsLink`, and `SseLink` (each is a `using` alias for `LinkOptions`):

```cpp
struct LinkOptions {
    std::string url;
    Headers headers;
    std::optional<std::string> caCert;  // PEM-encoded CA certificate for TLS
};

using HttpLinkOptions = LinkOptions;
using WsLinkOptions = LinkOptions;
using SseLinkOptions = LinkOptions;
```

### Built-in links

| Class       | Transport                | Protocol                                                                      |
|-------------|--------------------------|-------------------------------------------------------------------------------|
| `HttpLink`  | HTTP/HTTPS POST          | `application/json`; falls back to SSE (`text/event-stream`) for subscriptions |
| `WsLink`    | WebSocket                | `graphql-transport-ws`; auto-reconnects with back-off                         |
| `SseLink`   | Server-Sent Events       | `graphql-sse`                                                                 |
| `SplitLink` | Routes between two links | —                                                                             |

### `SplitLink`

```cpp
SplitLink(
    std::function<bool(const GraphQLRequest&)> predicate,
    std::shared_ptr<Link> left, // used when predicate returns true
    std::shared_ptr<Link> right // used when predicate returns false
);
```

---

## `gqlxy::Cache`

Abstract base for cache implementations.

```cpp
class Cache {
public:
    virtual std::optional<GraphQLResponse> Read(const GraphQLRequest& request) = 0;
    virtual void Write(const GraphQLRequest& request, const GraphQLResponse& result) = 0;
    virtual void Evict(const GraphQLRequest& request) = 0;
};
```

### `InMemoryCache`

Normalized, thread-safe entity store.

```cpp
InMemoryCache();
explicit InMemoryCache(const InMemoryCacheOptions& options);

void EvictEntity(const std::string& entityId);  // e.g. "User:42"
nlohmann::json Extract();                       // snapshot of the entity store
```

```cpp
struct TypePolicy {
    std::vector<std::string> keyFields = {"id"};
};

struct InMemoryCacheOptions {
    std::unordered_map<std::string, TypePolicy> typePolicies;
};
```

---

## `gqlxy::DocumentTransform`

```cpp
using DocumentTransform = std::function<parser::Document(const parser::Document&)>;
```

Transforms are applied to every operation document before it is sent. The default transform list contains `AddTypename`, which injects `__typename` into every selection set so the cache can identify object types. Pass `.documentTransforms = {}` to opt out.

### `AddTypename`

```cpp
#include <gqlxy/transforms/add_typename.h>

parser::Document AddTypename(const parser::Document& doc);
```
