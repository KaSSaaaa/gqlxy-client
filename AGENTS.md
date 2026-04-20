# GQLXY Client — Agent Instructions

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
./out/build/arm64-osx-debug/tests/unit/gqlxy_client_unit_tests --gtest_filter=SuiteName.TestName
./out/build/arm64-osx-debug/tests/e2e/gqlxy_client_e2e_tests --gtest_filter=SuiteName.TestName
```

Dependencies are managed via vcpkg (bootstrapped automatically by the preset). Key deps: `rxcpp`, `nlohmann-json`, `boost-beast`, `boost-url`, `openssl`, `cppgraphqlgen`, `gtest` (tests feature), `ftxui` (samples feature).

## Architecture

The public surface lives entirely in `include/gqlxy/`:

- **`client.h`** — `Client` is the entry point. Constructed with `ClientOptions` (`.link`, `.cache`, `.defaultFetchPolicy`, `.documentTransforms`). All methods take option structs and return `Observable<GraphQLResponse>`:
  - `Query(QueryOptions)` / `Mutation(MutationOptions)` — single result; `co_await` or `.subscribe()`
  - `Subscribe(SubscribeOptions)` — event stream; always `.subscribe()`
  - `Refetch(QueryOptions)` — like `Query` but always `NetworkOnly`
- **`observable.h`** — `Observable<T>`: thin wrapper around `rxcpp::observable<T>` with `operator co_await()` (resolves first value) and implicit conversion to `rxcpp::observable<T>` for rxcpp operators. `.subscribe()` overloads return `Subscription`.
- **`subscription.h`** — `Subscription` RAII handle returned by `.subscribe()`. Call `.Unsubscribe()` to cancel, `.IsActive()` to check state.
- **`link.h`** — `Link` pure virtual: `Execute(GraphQLRequest) → Observable<GraphQLResponse>`. All transports implement this.
- **`links/`** — Built-in link implementations:
  - `HttpLink` — HTTP/HTTPS POST via boost::beast; automatically streams SSE (`text/event-stream`) for subscriptions.
  - `WsLink` — persistent WebSocket using the `graphql-transport-ws` protocol; shared connection with auto-reconnect and back-off.
  - `SseLink` — Server-Sent Events via boost::beast using the `graphql-sse` protocol.
  - `SplitLink` — routes between two links via a `std::function<bool(const GraphQLRequest&)>` predicate.
- **`cache.h`** / **`cache/in_memory_cache.h`** — `Cache` pure virtual; `InMemoryCache` (thread-safe, normalised entity store, configurable `typePolicies` for custom key fields).
- **`client/results.h`** — `GraphQLRequest` (`.query`, `.variables`, `.operationName`, `.type`, `.policy`) and `GraphQLResponse` (`.data`, `.errors`).
- **`client/fetch_policy.h`** — `FetchPolicy` enum: `CacheFirst` (default), `NetworkOnly`, `CacheAndNetwork`, `NoCache`.
- **`print.h`** — `Print(Document) → string`: serializes a parsed AST back to a GraphQL query string.
- **`transforms/add_typename.h`** — `AddTypename(Document) → Document`: recursively injects `__typename` into every selection set. Applied by default via `ClientOptions::documentTransforms`.

`DocumentTransform` is `std::function<parser::Document(const parser::Document&)>`. The default transform list applies `AddTypename` so the cache can identify object types. Pass `.documentTransforms = {}` to opt out.

Internal implementation lives in `src/gqlxy/`. boost::beast is PRIVATE (not in public headers). rxcpp is PUBLIC (appears in `Link` and `Observable` signatures).

**Internal: `AsioContext`** (`src/gqlxy/internal/asio_context.h`) — singleton `io_context` on a background thread, shared by all link implementations.

**Internal: WS connection** — `WsConnectionContext` manages WebSocket lifecycle with states `Idle`, `Connecting`, `Connected`, `Reconnecting`. All state logic runs on the ASIO thread.

**Internal: HTTP streaming** — `IHttpStream` exposes `ReadHeader` + `ReadBodyChunk`. `HttpLink` reads the header first, detects `Content-Type: text/event-stream`, then either drains a full JSON body or loops with `ParseSseEvents()`.

## Key Conventions

**Client construction:**
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

**co_await style:**
```cpp
auto result = co_await client.Query({.query = R"( query { __typename } )"});
if (result.data) { ... }
```

**Subscribe style:**
```cpp
Subscription sub = client.Subscribe({.query = R"( subscription { onMessage { text } } )"})
    .subscribe(
        [](const GraphQLResponse& r) { ... },
        [](exception_ptr) { ... }
    );
sub.Unsubscribe();
```

**Custom Link:**
```cpp
class MyLink : public gqlxy::Link {
public:
    Observable<GraphQLResponse> Execute(const GraphQLRequest& request) override {
        return rxcpp::observable<>::create<GraphQLResponse>([](rxcpp::subscriber<GraphQLResponse> s) {
            s.on_next(...);
            s.on_completed();
        });
    }
};
```

**`operationName`** is auto-parsed from the query string by `Client`. Anonymous operations yield `nullopt`. Do not set it manually.

**`clang-format`** — style defined in `.clang-format` (column limit 120, pointer/ref left-aligned). Use `// clang-format off/on` around deeply-nested initializer blocks.

**Naming** — private members: `_name` (leading underscore). Static/constexpr: `UpperCamelCase`. No closing namespace comments.

**Commits** — [Conventional Commits](https://www.conventionalcommits.org/): `feat`, `fix`, `docs`, `refactor`, `test`, `chore`. Summary ≤ 72 chars, imperative mood.

## Tests

Two test binaries:
- **`gqlxy_client_unit_tests`** — fast, no network. May `#include` internal `src/` headers via `target_include_directories`.
- **`gqlxy_client_e2e_tests`** — spins up an in-process `gqlxy-server` on ports 4001 (HTTP/WS) and 4002 (HTTPS/WSS). Tests use `TEST_P` over `LinkParam` (HTTP, WS, SplitLink, SSL variants).

**Test utilities in `tests/`:**
- **`to_result.h`** — `to_result(Observable<T>)` blocks until `on_completed` or `on_error`. Returns `Result<T>` with `.values`, `.exception`, `.completed`.
- **`ASSERT_GQL_SUCCESS(out)`** — asserts no exception, at least one value, no GraphQL errors, data present.

Use `TEST_F` (shared fixture) and `TEST_P` (parameterized) to eliminate duplication.

## Code Quality

- Don't comment code unless it needs clarification — code must be self-readable
- Favor structs over multiple parameters (`HttpLinkOptions`, `ClientOptions`, `QueryOptions`, ...)
- Functions ≤ 20 lines
- `std::optional` over raw pointers; no raw owning pointers; `const&` by default
- No `friend` declarations; no private inheritance
- Designated initializer fields on their own lines, no padding to align `=` signs
