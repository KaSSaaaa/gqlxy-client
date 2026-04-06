# GQLXY Client — Roadmap

This document tracks what is already in place and what remains to be built.

---

## What works today

- Project scaffold: CMake + vcpkg, C++20, cross-platform presets (arm64/x64 macOS, Linux, Windows)
- Public API defined: `Client`, `Link`, `Cache`, `GraphQLRequest`, `GraphQLResult`, `Task<T>`
- Link abstractions: `HttpLink`, `WsLink`, `SplitLink` (implemented); `SseLink` (merged into `HttpLink`)
- `InMemoryCache` — thread-safe normalized result cache
- Observable API: `Client::Query()` / `Client::Mutation()` / `Client::Subscribe()` returning `Observable<GraphQLResult>` (supports both `co_await` and `.subscribe()`)
- **P1 complete**: `HttpLink` — HTTP/HTTPS queries & mutations + SSE subscriptions via Boost.Beast
- **P3 complete**: `WsLink` — persistent WebSocket connection shared across requests, reconnect with back-off, concurrent subscriptions with distinct IDs
- Unit tests: `InMemoryCache`, `HttpLink`, `WsLink`
- E2E tests: HTTP queries, SSE subscriptions, WS queries & subscriptions, WS connection reuse & concurrent subscriptions (against in-process gqlxy-server)

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

## P5 — Cache Integration

Connect `InMemoryCache` to the client execution path.

| # | Feature | Notes |
|---|---------|-------|
| 15 | **Cache-first policy** | Check cache before executing; skip network on hit |
| 16 | **Cache-and-network** | Return cached result immediately, then fetch and update |
| 17 | **Cache invalidation** | `Client::Refetch()` to bypass cache for a specific request |

---

## Suggested implementation order

```
Phase 1 — HttpLink (+ SSE)    : done
Phase 2 — WsLink               : done
Phase 3 — Persistent WsLink    : done
Phase 4 — Async coroutines     : done
Phase 5 — Cache integration    : #15, #16, #17
```

