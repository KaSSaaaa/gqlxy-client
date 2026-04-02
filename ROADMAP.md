# GQLXY Client — Roadmap

This document tracks what is already in place and what remains to be built.

---

## What works today

- Project scaffold: CMake + vcpkg, C++20, cross-platform presets (arm64/x64 macOS, Linux, Windows)
- Public API defined: `Client`, `Link`, `Cache`, `GraphQLRequest`, `GraphQLResult`, `Task<T>`
- Link abstractions: `HttpLink`, `WsLink`, `SseLink`, `SplitLink` (interfaces + stubs)
- `InMemoryCache` — thread-safe normalized result cache
- Coroutine API: `Client::Query()` / `Client::Mutation()` returning `Task<GraphQLResult>`
- RxCpp API: `Client::QueryRx()` / `Client::MutationRx()` / `Client::Subscribe()` returning `rxcpp::observable<GraphQLResult>`
- Unit tests for `InMemoryCache`

---

## P1 — HttpLink

Implement HTTP transport via `boost::beast` for query and mutation execution.

| # | Feature | Notes |
|---|---------|-------|
| 1 | **POST /graphql** | Serialize `GraphQLRequest` to JSON, send via `beast::http::async_write`, parse response |
| 2 | **TLS support** | `https://` URLs via `boost::asio::ssl::stream` + OpenSSL |
| 3 | **Error mapping** | HTTP 4xx/5xx → `GraphQLResult.errors`; network errors → `on_error` |
| 4 | **Custom headers** | `HttpLinkOptions::headers` map forwarded with every request |

---

## P2 — WsLink

Implement WebSocket transport using the `graphql-transport-ws` protocol.

| # | Feature | Notes |
|---|---------|-------|
| 5 | **Connection lifecycle** | `connection_init` / `connection_ack` handshake on first use |
| 6 | **subscribe / next / complete** | Map to RxCpp `on_next` / `on_completed` |
| 7 | **error messages** | Map to RxCpp `on_error` |
| 8 | **ping / pong** | Respond to server pings to keep connection alive |
| 9 | **TLS support** | `wss://` via `boost::asio::ssl::stream` |

---

## P3 — SseLink

Implement Server-Sent Events transport using the `graphql-sse` distinct-connections protocol.

| # | Feature | Notes |
|---|---------|-------|
| 10 | **SSE stream parsing** | Parse `event:` / `data:` lines into `GraphQLResult` objects |
| 11 | **Connection init** | Send POST with `Accept: text/event-stream` |
| 12 | **Reconnect on drop** | Auto-reconnect with `Last-Event-ID` header |
| 13 | **TLS support** | `https://` via OpenSSL |

---

## P4 — Async Coroutine Integration

Wire `Task<T>` into the boost::asio event loop so that `Query()` / `Mutation()` truly suspend the coroutine instead of blocking.

| # | Feature | Notes |
|---|---------|-------|
| 14 | **asio coroutine executor** | Use `boost::asio::co_spawn` / `boost::asio::use_awaitable` |
| 15 | **Task awaits observable** | Bridge `rxcpp::observable` to `co_await` via a single-value adapter |

---

## P5 — Cache Integration

Connect `InMemoryCache` to the client execution path.

| # | Feature | Notes |
|---|---------|-------|
| 16 | **Cache-first policy** | Check cache before executing; skip network on hit |
| 17 | **Cache-and-network** | Return cached result immediately, then fetch and update |
| 18 | **Cache invalidation** | `Client::Refetch()` to bypass cache for a specific request |

---

## Suggested implementation order

```
Phase 1 — HttpLink          : #1, #2, #3, #4
Phase 2 — WsLink            : #5, #6, #7, #8, #9
Phase 3 — SseLink           : #10, #11, #12, #13
Phase 4 — Async coroutines  : #14, #15
Phase 5 — Cache integration : #16, #17, #18
```
