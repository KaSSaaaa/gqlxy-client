# GQLXY Client — Copilot Instructions

Communicate at a senior C++ engineer level with deep GraphQL knowledge.

## Build & Test

**Configure and build** (macOS arm64):
```sh
cmake --preset arm64-osx-debug
cmake --build out/build/arm64-osx-debug
```

Other presets: `x64-osx-debug`, `x64-linux-debug`, `x64-windows-debug`.

**Run all tests:**
```sh
ctest --test-dir out/build/arm64-osx-debug --output-on-failure
```

**Run a single test:**
```sh
./out/build/arm64-osx-debug/tests/gqlxy_client_unit_tests --gtest_filter=SuiteName.TestName
./out/build/arm64-osx-debug/tests/gqlxy_client_e2e_tests --gtest_filter=SuiteName.TestName
```

Dependencies are managed via vcpkg (bootstrapped automatically by the preset). Key deps: `rxcpp`, `nlohmann-json`, `boost-beast`, `boost-url`, `openssl`, `gtest` (tests feature).

## Architecture

The public surface lives entirely in `include/gqlxy/`:

- **`client.h`** — `Client` is the entry point. Constructed with `ClientOptions` (`.link` + `.cache`). All three methods return `Observable<GraphQLResult>`:
  - `Query()` / `Mutation()` — `co_await` for the single result, or `.subscribe()` for reactive style
  - `Subscribe()` — `.subscribe()` for the event stream
- **`observable.h`** — `Observable<T>`: a thin wrapper around `rxcpp::observable<T>` that adds `operator co_await()`. Awaiting resolves the first emitted value. Also implicitly converts to `rxcpp::observable<T>` for access to rxcpp operators (`map`, `filter`, `merge`, etc.).
- **`link.h`** — `Link` is a pure virtual interface: `Execute(GraphQLRequest) → Observable<GraphQLResult>`. All transport adapters implement this.
- **`links/`** — Built-in link implementations:
  - `HttpLink` — HTTP/HTTPS POST for queries & mutations; SSE for subscriptions (all via boost::beast)
  - `WsLink` — persistent WebSocket connection using the `graphql-transport-ws` protocol; shared across requests with auto-reconnect and back-off
  - `SplitLink` — routes between two links via a `std::function<bool(const GraphQLRequest&)>` predicate
- **`cache.h`** / **`cache/in_memory_cache.h`** — `Cache` pure virtual interface; `InMemoryCache` is the default (thread-safe, keyed on `query` + `variables` JSON). **Cache integration into the execution path is not yet implemented (P5).**
- **`client/results.h`** — `GraphQLRequest` (query, variables, operationName, **type**) and `GraphQLResult` (data, errors). `OperationType` enum (`Query`, `Mutation`, `Subscription`) on `GraphQLRequest` is the idiomatic predicate for `SplitLink`.
- **`task.h`** — `Task<T>`: minimal C++20 coroutine type shared with gqlxy-server

Internal implementation lives in `src/gqlxy/`. boost::beast is a PRIVATE dependency (not exposed in public headers). rxcpp is PUBLIC (appears in `Link` and `Client` signatures).

**Internal: `AsioContext`** (`src/gqlxy/internal/asio_context.h`) — singleton `io_context` running on a background thread, shared by all link implementations. `AsioContext::OnContext()` returns `true` when called from that thread (used by `WsConnection` destructor to avoid deadlocks).

**Internal: WS state machine** — `WsConnection` delegates to `WsConnectionContext`, which drives states in `src/gqlxy/internal/ws/connection/state/`: `IdleState` → `ConnectingState` → `ConnectedState` ↔ `ReconnectingState`. Each state implements `IConnectionState`.

## Key Conventions

**Client construction** (designated initializers):
```cpp
gqlxy::Client client({
    .link = make_shared<SplitLink>(
        [](const GraphQLRequest& req) { return req.type != OperationType::Subscription; },
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

**Advanced rxcpp operators** — assign to `rxcpp::observable<GraphQLResult>` via implicit conversion:
```cpp
rxcpp::observable<GraphQLResult> obs = client.Query("...");
obs.filter([](auto& r) { return r.data.has_value(); }).subscribe(...);
```

**Implementing a custom Link:**
```cpp
class MyLink : public gqlxy::Link {
public:
    Observable<GraphQLResult> Execute(const GraphQLRequest& request) override {
        return rxcpp::observable<>::create<GraphQLResult>([&](rxcpp::subscriber<GraphQLResult> s) {
            // ... produce results via s.on_next(), s.on_completed(), s.on_error()
        });
    }
};
```

**`clang-format`** — style defined in `.clang-format` (column limit 120, pointer/ref alignment left, no closing namespace comments). Use `// clang-format off/on` around deeply-nested initializer blocks.

**Naming** — private member variables use leading underscore (`_name`). Static/constexpr variables use UpperCamelCase (no `k_` prefix). No closing namespace comments.

**Commits** — use [Conventional Commits](https://www.conventionalcommits.org/): `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, etc. Summary line ≤ 72 chars, imperative mood.

## Tests

Two test binaries:
- **`gqlxy_client_unit_tests`** — fast, no network. Tests may include `src/` internal headers.
- **`gqlxy_client_e2e_tests`** — spins up an in-process `gqlxy-server` on port 4001 via `ServerEnvironment` (GTest global environment). Tests run against HTTP, WS, and `SplitLink` via `TEST_P` parameterization.

Key test utilities in `tests/`:
- **`to_result(Observable<T>)`** — blocks with mutex+condvar until `on_completed` or `on_error`; returns `Result<T>` with `.values`, `.exception`, `.completed`.
- **`ASSERT_GQL_SUCCESS(out)`** — asserts no exception, at least one value, no GraphQL errors, data present.

Use `TEST_F` (shared fixture) and `TEST_P` (parameterized) to eliminate duplication.

## Code Quality

- Don't comment code unless necessary — code must be readable by itself
- Favor structs over multiple parameters (e.g. `HttpLinkOptions`, `ClientOptions`)
- Functions ≤ 20 lines
- Use DRY principles
- `std::optional` over raw pointers; no raw owning pointers; `const&` by default
- No `friend` declarations; no private inheritance
