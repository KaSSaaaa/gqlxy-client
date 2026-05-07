# GQLXY Client — Roadmap

This document tracks what is already in place and what remains to be built.

---

## What works today

- Project scaffold: CMake + vcpkg, C++20, cross-platform presets (arm64/x64 macOS, Linux, Windows)
- Public API defined: `Client`, `Link`, `Cache`, `GraphQLRequest`, `GraphQLResult`, `Task<T>`
- Link abstractions: `HttpLink`, `WsLink`, `SplitLink` (implemented); `SseLink` (merged into `HttpLink`)
- `InMemoryCache` — Apollo-style normalized entity cache with `__typename:id` keying, type policies, and `Extract()` for store inspection
- Observable API: `Client::Query()` / `Client::Mutation()` / `Client::Subscribe()` returning `Observable<GraphQLResult>` (supports both `co_await` and `.subscribe()`)
- Fetch policies: `CacheFirst`, `NetworkOnly`, `CacheAndNetwork`, `NoCache` + `Client::Refetch()`
- **P1 complete**: `HttpLink` — HTTP/HTTPS queries & mutations + SSE subscriptions via Boost.Beast
- **P3 complete**: `WsLink` — persistent WebSocket connection shared across requests, reconnect with back-off, concurrent subscriptions with distinct IDs
- Unit tests: `InMemoryCache` (normalized), query parser, `HttpLink`, `WsLink`
- E2E tests: HTTP queries, SSE subscriptions, WS queries & subscriptions, WS connection reuse, cache policies (against in-process gqlxy-server)

---

## ~~P1 — HttpLink~~ ✅ Done

Implement HTTP transport via `boost::beast` for query and mutation execution.

| # | Feature | Status |
|---|---------|--------|
| 1 | **POST /graphql** | ✅ |
| 2 | **TLS support** (`https://`) | ✅ |
| 3 | **Error mapping** (HTTP 4xx/5xx + network errors) | ✅ |
| 4 | **Custom headers** | ✅ |
| — | **SSE subscriptions** (merged from P3) | ✅ |

---

## ~~P2 — WsLink~~ ✅ Done

Implement WebSocket transport using the `graphql-transport-ws` protocol.

| # | Feature | Status |
|---|---------|--------|
| 5 | **Connection lifecycle** (`connection_init` / `connection_ack`) | ✅ |
| 6 | **subscribe / next / complete** | ✅ |
| 7 | **error messages** | ✅ |
| 8 | **ping / pong** | ✅ |
| 9 | **TLS support** (`wss://`) | ✅ |

---

## ~~P3 — Persistent WsLink connection~~ ✅ Done

The current WsLink opens a fresh WebSocket per `Execute()` call. A persistent
connection (shared across requests, with reconnect logic) is needed for
production use.

| # | Feature | Notes |
|---|---------|-------|
| 10 | **Shared connection** | One WS connection per `WsLink` instance, reused across requests | ✅ |
| 11 | **Reconnect on drop** | Auto-reconnect with back-off; replay in-flight subscriptions | ✅ |
| 12 | **Concurrent subscriptions** | Multiple in-flight `subscribe` messages with distinct IDs | ✅ |

---

## ~~P4 — Async Coroutine Integration~~ ✅ Done

Wire `Task<T>` into the boost::asio event loop so that `Query()` / `Mutation()` truly suspend the coroutine instead of blocking.

| # | Feature | Notes |
|---|---------|-------|
| 13 | **asio coroutine executor** | Shared `AsioContext` singleton; `HttpLink` uses `boost::asio::co_spawn` / `boost::asio::use_awaitable` | ✅ |
| 14 | **Task awaits observable** | Bridge `Observable` to `co_await` via a single-value adapter | ✅ |

---

## ~~P5 — Cache Integration~~ ✅ Done

Apollo-style `InMemoryCache` with normalized entity storage, fetch policies, and `Client::Refetch()`.

| # | Feature | Status |
|---|---------|--------|
| 15 | **Cache-first policy** | ✅ |
| 16 | **Cache-and-network** | ✅ |
| 17 | **Cache invalidation** | ✅ |

---

## P6 — Reactive cache + watched queries

Apollo's `watchQuery` keeps a live `ObservableQuery` that re-emits whenever cached data changes. This requires a cache subscriber model.

| # | Feature | Notes |
|---|---------|-------|
| 18 | **Cache change notifications** | `InMemoryCache` broadcasts entity-level diffs to interested observers |
| 19 | **`Client::WatchQuery()`** | Returns an `Observable` that re-emits on every relevant cache update |
| 20 | **Field-level diff / merge** | Only re-emit when the fields selected by the query actually changed |
| 21 | **`ObservableQuery` handle** | Exposes `.Refetch()`, `.FetchMore()`, `.UpdateQuery()` on the live stream |

---

## P7 — Pagination

Apollo provides `fetchMore` + field-level merge policies for cursor- and offset-based pagination.

| # | Feature | Notes |
|---|---------|-------|
| 22 | **`Client::FetchMore()`** | Executes a follow-up query and merges its result into the existing cached data |
| 23 | **`read` / `merge` field policies** | Per-field callbacks on `TypePolicy` that control how incoming data is merged into the store |
| 24 | **`relayStylePagination` helper** | Out-of-the-box Relay cursor pagination merge policy |
| 25 | **`offsetLimitPagination` helper** | Out-of-the-box offset/limit pagination merge policy |

---

## P8 — Optimistic UI

Apollo allows mutations to immediately write an optimistic response to the cache, which is rolled back once the real result arrives.

| # | Feature | Notes |
|---|---------|-------|
| 26 | **`MutationOptions::optimisticResponse`** | Speculative `json` written to cache before the network round-trip |
| 27 | **Optimistic layer in `InMemoryCache`** | Separate overlay store that is applied on top of the canonical store |
| 28 | **Automatic rollback** | Overlay removed and canonical data re-applied when the mutation resolves or errors |
| 29 | **`update` callback** | Post-mutation callback for manual cache writes (e.g. appending to a list) |

---

## P9 — Error handling & retry

| # | Feature | Notes |
|---|---------|-------|
| 30 | **`RetryLink`** | Configurable retry with back-off for transient network errors |
| 31 | **`ErrorLink`** | Intercept and transform errors in the link chain (e.g. refresh token on 401) |
| 32 | **Partial data + errors** | Surface `GraphQLResponse` with both `.data` and `.errors` when the server returns both |
| 33 | **`onError` hook in `ClientOptions`** | Global error handler invoked for every error response |

---

## P10 — Link composition utilities

| # | Feature | Notes |
|---|---------|-------|
| 34 | **`ApolloLink::from()`** — `concat`** | Chain multiple links into a pipeline |
| 35 | **`ContextLink`** | Inject per-request context (e.g. auth token) without subclassing |
| 36 | **`BatchHttpLink`** | Coalesce multiple concurrent queries into a single HTTP request |
| 37 | **`PersistedQueryLink`** | Automatic APQ (Automatic Persisted Queries) using SHA-256 hashing |

---

## P11 — Local state & resolvers

Apollo Client supports client-side `@client` fields resolved entirely from local state.

| # | Feature | Notes |
|---|---------|-------|
| 38 | **`@client` directive stripping** | Remove `@client` fields from the query sent to the server |
| 39 | **Local resolver map** | Per-field callbacks that resolve `@client` fields from the cache or arbitrary state |
| 40 | **`writeFragment` / `readFragment`** | Granular cache access by fragment rather than by full query |
| 41 | **`writeQuery` / `readQuery`** | Direct programmatic cache reads/writes without going through `Client` |

---

## P12 — Developer experience

| # | Feature | Notes |
|---|---------|-------|
| 42 | **`InMemoryCache::gc()`** | Garbage-collect unreachable entities (not referenced by any root query) |
| 43 | **`InMemoryCache::restore(json)`** | Hydrate the store from a serialised snapshot (SSR / persistence) |
| 44 | **Devtools integration** | Structured cache snapshot emitted over a debug channel for tooling |
| 45 | **Configurable logger / tracer** | Pluggable `LogLink` for request/response tracing |

---

## Suggested implementation order

```
Phase 1 — HttpLink (+ SSE)         : done
Phase 2 — WsLink                    : done
Phase 3 — Persistent WsLink         : done
Phase 4 — Async coroutines          : done
Phase 5 — Cache integration         : done
Phase 6 — Reactive cache            : next
Phase 7 — Pagination                : after P6
Phase 8 — Optimistic UI             : after P7
Phase 9 — Error handling & retry    : can be parallelised with P7/P8
Phase 10 — Link composition         : can be parallelised with P7/P8
Phase 11 — Local state              : after P6
Phase 12 — Developer experience     : ongoing
```
