# API Reference

## `gqlxy::Client`

Top-level entry point. Configured with a `Link` and an optional `Cache`.

```cpp
struct ClientOptions {
    std::shared_ptr<Link> link;
    std::shared_ptr<Cache> cache;
};

explicit Client(ClientOptions options);
```

### Methods

| Method | Returns | Description |
|--------|---------|-------------|
| `Query(query, variables)` | `Task<GraphQLResult>` | Execute a query (coroutine) |
| `Mutation(query, variables)` | `Task<GraphQLResult>` | Execute a mutation (coroutine) |
| `QueryRx(query, variables)` | `rxcpp::observable<GraphQLResult>` | Execute a query (reactive) |
| `MutationRx(query, variables)` | `rxcpp::observable<GraphQLResult>` | Execute a mutation (reactive) |
| `Subscribe(query, variables)` | `rxcpp::observable<GraphQLResult>` | Open a subscription stream |

---

## `gqlxy::Link`

Abstract base for transport adapters.

```cpp
virtual rxcpp::observable<GraphQLResult> Execute(const GraphQLRequest& request) = 0;
```

### Built-in Links

| Class | Transport |
|-------|-----------|
| `HttpLink` | HTTP POST (`application/json`) |
| `WsLink` | WebSocket (`graphql-transport-ws`) |
| `SseLink` | Server-Sent Events (`graphql-sse`) |
| `SplitLink` | Routes between two links via a predicate |

---

## `gqlxy::Cache`

Abstract base for result caching.

```cpp
virtual std::optional<GraphQLResult> Read(const GraphQLRequest& request) = 0;
virtual void Write(const GraphQLRequest& request, const GraphQLResult& result) = 0;
virtual void Evict(const GraphQLRequest& request) = 0;
```

`InMemoryCache` is the default implementation (thread-safe, keyed by `query` + `variables`).

---

## `gqlxy::GraphQLRequest`

```cpp
struct GraphQLRequest {
    std::string query;
    nlohmann::json variables = nullptr;
    std::optional<std::string> operationName;
};
```

## `gqlxy::GraphQLResult`

```cpp
struct GraphQLResult {
    std::optional<nlohmann::json> data;
    std::optional<GraphQLErrors> errors;
};
```

## `gqlxy::Task<T>`

Minimal C++20 coroutine type. Use `co_await` inside a coroutine, or `.get()` for synchronous resolution.
