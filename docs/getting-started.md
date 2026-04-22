# Getting Started

This guide walks you through installing GQLXY Client, wiring up a link and cache, and executing your first query.

## Prerequisites

- A C++20-capable compiler (Clang 14+, GCC 12+, MSVC 19.30+)
- [CMake](https://cmake.org/) 3.14+
- [Ninja](https://ninja-build.org/) (recommended)

## Installation

### FetchContent

```cmake
cmake_minimum_required(VERSION 3.14)
project(my_graphql_app)

include(FetchContent)

FetchContent_Declare(
    gqlxy_client
    GIT_REPOSITORY https://github.com/anomalyco/gqlxy-client.git
    GIT_TAG        main
)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(gqlxy_client)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE gqlxy::client)
```

### From source

```bash
git clone https://github.com/anomalyco/gqlxy-client.git
cd gqlxy-client
```

GQLXY Client ships CMake presets for every major platform:

| Platform | Preset |
|---|---|
| macOS Apple Silicon | `arm64-osx-debug` / `arm64-osx-release` |
| macOS Intel | `x64-osx-debug` / `x64-osx-release` |
| Linux x64 | `x64-linux-debug` / `x64-linux-release` |
| Windows x64 | `x64-windows-debug` / `x64-windows-release` |

```bash
# Configure (downloads dependencies via vcpkg automatically)
cmake --preset arm64-osx-debug

# Build
cmake --build out/build/arm64-osx-debug
```

To run the tests:

```bash
ctest --test-dir out/build/arm64-osx-debug --output-on-failure
```

## Constructing a Client

A `Client` requires at minimum a `Link` — the transport layer. Optionally provide a `Cache` for normalized caching.

```cpp
#include <gqlxy/client.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/cache/in_memory_cache.h>

using namespace gqlxy;

Client client({
    .link  = std::make_shared<HttpLink>(HttpLinkOptions{.url = "http://localhost:4000/graphql"}),
    .cache = std::make_shared<InMemoryCache>()
});
```

For applications with both queries/mutations (HTTP) and subscriptions (WebSocket), use a `SplitLink` to route automatically:

```cpp
#include <gqlxy/links/split_link.h>
#include <gqlxy/links/ws_link.h>

Client client({
    .link = std::make_shared<SplitLink>(
        [](const GraphQLRequest& req) {
            return req.type != parser::OperationType::SUBSCRIPTION;
        },
        std::make_shared<HttpLink>(HttpLinkOptions{.url = "http://localhost:4000/graphql"}),
        std::make_shared<WsLink>(WsLinkOptions{.url  = "ws://localhost:4000/graphql"})
    ),
    .cache = std::make_shared<InMemoryCache>()
});
```

## Executing a query

### co_await style (C++20 coroutine)

```cpp
boost::asio::awaitable<void> run(Client& client) {
    auto result = co_await client.Query({
        .query = R"(
            query GetUser($id: ID!) {
                user(id: $id) {
                    id
                    name
                }
            }
        )",
        .variables = {{"id", "42"}}
    });

    if (result.errors) {
        for (const auto& e : *result.errors)
            std::cerr << e.message << "\n";
        co_return;
    }

    std::cout << result.data->dump(2) << "\n";
}
```

`Client::Query()` returns `Observable<GraphQLResponse>`. Awaiting it resolves the first (and only) value from the stream.

### subscribe style (reactive)

```cpp
auto sub = client.Query({
    .query = "{ hello }"
}).subscribe(
    [](const GraphQLResponse& result) {
        std::cout << result.data->dump(2) << "\n";
    },
    [](std::exception_ptr e) {
        try { std::rethrow_exception(e); }
        catch (const std::exception& ex) { std::cerr << ex.what() << "\n"; }
    }
);
```

## Executing a mutation

```cpp
auto result = co_await client.Mutation({
    .query = R"(
        mutation CreatePost($title: String!, $body: String!) {
            createPost(title: $title, body: $body) {
                id
                title
            }
        }
    )",
    .variables = {{"title", "Hello"}, {"body", "World"}}
});
```

Mutations always use `NetworkOnly` and bypass the cache.

## Subscribing to events

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

`Subscribe()` returns a `Subscription` RAII handle. Call `.Unsubscribe()` to stop receiving events. The handle is also cancelled when it goes out of scope.
