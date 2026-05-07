---
title: Caching
---

`InMemoryCache` is a normalized, thread-safe entity store. When a query response arrives, every object with a resolvable identity
is extracted into a flat map keyed by `__typename:id`. Subsequent queries that overlap the same entities read from the cache
instead of the network, and cache updates propagate immediately to all queries that share those entities.

## Basic setup

```cpp
#include <gqlxy/cache/in_memory_cache.h>

auto client = gqlxy::Client({
    .link  = make_shared<HttpLink>(HttpLinkOptions{
        .url = "http://localhost:4000/graphql"
    }),
    .cache = make_shared<InMemoryCache>()
});
```

No further configuration is required. By default, every object with an `id` field is normalized under its `__typename`.

## Fetch policies

The `FetchPolicy` controls how `Query` and `Refetch` interact with the cache.

| Policy                   | Read cache | Write cache | Behaviour                                                              |
|--------------------------|------------|-------------|------------------------------------------------------------------------|
| `CacheFirst` *(default)* | Yes        | Yes         | Returns cached data immediately; skips the network if the entry exists |
| `NetworkOnly`            | No         | Yes         | Always fetches from the network; writes the result to the cache        |
| `CacheAndNetwork`        | Yes        | Yes         | Emits cached data first, then re-fetches and emits the network result  |
| `NoCache`                | No         | No          | Always fetches from the network; result is not stored                  |

Set a client-wide default via `ClientOptions::defaultFetchPolicy`, or override per request:

```cpp
// Client-wide default
auto client = gqlxy::Client({
    .link = ...,
    .cache = make_shared<InMemoryCache>(),
    .defaultFetchPolicy = FetchPolicy::NetworkOnly
});

// Per-request override
auto result = co_await client.Query({
    .query = R"(
        query {
            user(id: "1") {
                name
            }
        }
    )",
    .fetchPolicy = FetchPolicy::CacheAndNetwork
});
```

`Mutation` ignores the fetch policy and always goes to the network. `Subscribe` never touches the cache by itself. You can
still use their results to alter the cache by yourself.

## Type policies

By default, the cache identifies objects by their `__typename` and `id` field. To use a different key or composite keys, supply a `TypePolicy` in `InMemoryCacheOptions`:

```cpp
#include <gqlxy/cache/type_policy.h>

auto cache = make_shared<InMemoryCache>(InMemoryCacheOptions{
    .typePolicies = {
        {"Product", TypePolicy{.keyFields = {"sku", "region"}}},
        {"Session", TypePolicy{.keyFields = {"token"}}}
    }
});
```

Objects of type `Product` are now keyed by `sku` + `region` concatenated, and `Session` objects by `token`.

## Manual cache operations

### Evicting a single query

```cpp
cache->Evict(GraphQLRequest{
    .query = R"(
        query {
            posts {
                id
                title
            }
        }
    )"
});
```

### Evicting a normalized entity

```cpp
cache->EvictEntity("User:42");
```

Any query whose result references that entity will return a cache miss on the next read.

### Inspecting the raw store

```cpp
nlohmann::json store = cache->Extract();
std::cout << store.dump(2) << std::endl;
```

`Extract()` returns a snapshot of the internal entity map as a JSON object. Useful for debugging and for serializing cache state.

## Bypassing the cache

Pass `FetchPolicy::NoCache` to opt out entirely for a specific operation:

```cpp
auto result = co_await client.Query({
    .query = R"(
        query {
            livePrice {
                usd
            }
        }
    )",
    .fetchPolicy = FetchPolicy::NoCache
});
```

`Refetch()` is a convenience wrapper that calls `Query` with `FetchPolicy::NetworkOnly`, always hitting the network and updating the cache:

```cpp
auto fresh = co_await client.Refetch({
    .query = R"(
        query {
            user(id: "1") {
                name
            }
        }
    )"
});
```
