# GQLXY Client — Copilot Instructions

Communicate at a senior C++ engineer level with deep GraphQL knowledge.

## Build & Test

**Configure and build** (macOS arm64):
```sh
cmake --preset arm64-osx-debug
cmake --build out/build/arm64-osx-debug
```

**Run all tests:**
```sh
ctest --test-dir out/build/arm64-osx-debug --output-on-failure
```

**Run a single test:**
```sh
./out/build/arm64-osx-debug/tests/gqlxy_client_unit_tests --gtest_filter=SuiteName.TestName
```

Dependencies are managed via vcpkg (bootstrapped automatically by the preset). Key deps: `rxcpp`, `nlohmann-json`, `boost-beast`, `openssl`, `gtest` (tests feature).

## Architecture

The public surface lives entirely in `include/gqlxy/`:

- **`client.h`** — `Client` is the entry point. Constructed with `ClientOptions` (`.link` + `.cache`). All three methods return `Observable<GraphQLResult>`:
  - `Query()` / `Mutation()` — `co_await` for the single result, or `.subscribe()` for reactive style
  - `Subscribe()` — `.subscribe()` for the event stream
- **`observable.h`** — `Observable<T>`: a thin wrapper around `rxcpp::observable<T>` that adds `operator co_await()`. Awaiting resolves the first emitted value. Also implicitly converts to `rxcpp::observable<T>` for access to rxcpp operators (`map`, `filter`, `merge`, etc.).
- **`link.h`** — `Link` is a pure virtual interface: `Execute(GraphQLRequest) → rxcpp::observable<GraphQLResult>`. All transport adapters implement this.
- **`links/`** — Built-in link implementations:
  - `HttpLink` — HTTP POST via boost::beast
  - `WsLink` — WebSocket via boost::beast using the `graphql-transport-ws` protocol
  - `SseLink` — Server-Sent Events via boost::beast using the `graphql-sse` protocol
  - `SplitLink` — routes between two links via a `std::function<bool(const GraphQLRequest&)>` predicate
- **`cache.h`** — `Cache` pure virtual interface; `InMemoryCache` is the default (thread-safe, keyed on `query` + `variables` JSON)
- **`results.h`** — `GraphQLRequest` (query, variables, operationName) and `GraphQLResult` (data, errors)
- **`task.h`** — `Task<T>`: minimal C++20 coroutine type shared with gqlxy-server

Internal implementation lives in `src/gqlxy/`. boost::beast is a PRIVATE dependency (not exposed in public headers). rxcpp is PUBLIC (appears in `Link` and `Client` signatures).

## Key Conventions

**Client construction** (designated initializers):
```cpp
gqlxy::Client client({
    .link = make_shared<SplitLink>(
        [](const GraphQLRequest& req) { return req.query.find("subscription") == string::npos; },
        make_shared<HttpLink>(HttpLinkOptions{.url = "http://localhost:4000/graphql"}),
        make_shared<WsLink>(WsLinkOptions{.url = "ws://localhost:4000/graphql"})
    ),
    .cache = make_shared<InMemoryCache>()
});
```

**co_await style** — suspends the coroutine until the first value is emitted:
```cpp
auto result = co_await client.Query(R"( query { __typename } )");
if (result.data) { ... }
else if (result.errors) { ... }
```

**Subscribe style** — works for all three methods; mandatory for subscriptions:
```cpp
client.Query(R"( query { __typename } )")
    .subscribe([](const GraphQLResult& r) { ... });

client.Subscribe(R"( subscription { onMessage { text } } )")
    .subscribe(
        [](const GraphQLResult& r) { ... },   // on_next
        [](exception_ptr) { ... }              // on_error
    );
```

**Advanced rxcpp operators** — use `.operator rxcpp::observable<T>()` or assign to `rxcpp::observable<GraphQLResult>`:
```cpp
rxcpp::observable<GraphQLResult> obs = client.QueryRx("...");
obs.filter([](auto& r) { return r.data.has_value(); }).subscribe(...);
```

**Implementing a custom Link:**
```cpp
class MyLink : public gqlxy::Link {
public:
    rxcpp::observable<GraphQLResult> Execute(const GraphQLRequest& request) override {
        return rxcpp::observable<>::create<GraphQLResult>([&](rxcpp::subscriber<GraphQLResult> s) {
            // ... produce results via s.on_next(), s.on_completed(), s.on_error()
        });
    }
};
```

**`clang-format`** — style defined in `.clang-format`. Use `// clang-format off/on` around deeply-nested initializer blocks.

**Tests** — one binary: `gqlxy_client_unit_tests`. Tests may include `src/` internal headers via `target_include_directories(...PRIVATE ${CMAKE_SOURCE_DIR}/src)`.

**Code quality**
- Don't comment code unless necessary — code must be readable by itself
- Favor structs over multiple parameters (e.g. `HttpLinkOptions`, `ClientOptions`)
- Functions ≤ 20 lines
- Use DRY principles
- `std::optional` over raw pointers; no raw owning pointers; `const&` by default
