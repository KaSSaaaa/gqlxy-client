# Getting Started

This guide walks you through installing GQLXY Client, wiring up a link and cache, and executing your first query.

## Prerequisites

- A C++20-capable compiler (Clang 14+, GCC 12+, MSVC 19.30+)
- [CMake](https://cmake.org/) 3.14+
- [Ninja](https://ninja-build.org/) (recommended)

## Installation

### From source

```bash
git clone https://github.com/KaSSaaaa/gqlxy-client.git
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

### FetchContent

```cmake
cmake_minimum_required(VERSION 3.14)
project(my_graphql_app)

include(FetchContent)

FetchContent_Declare(
    gqlxy_client
    GIT_REPOSITORY https://github.com/KaSSaaaa/gqlxy-client.git
    GIT_TAG        main
)

set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(gqlxy_client)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE gqlxy::client)
```

## Building your first Client

A `Client` requires at minimum a `Link`, the transport layer. Optionally, you can provide a `Cache` for normalized caching.

```cpp
#include <gqlxy/client.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/cache/in_memory_cache.h>

using namespace std;
using namespace gqlxy;

Client client({
    .link = make_shared<HttpLink>(HttpLinkOptions{
        .url = "http://localhost:4000/graphql"
    }),
    .cache = make_shared<InMemoryCache>()
});
```

The `HttpLink` supports both the traditional `graphql-http` for queries and mutations and `graphql-sse` for subscriptions.

If you need websocket support, you can use a `WsLink`. It works for both queries/mutations and for subscriptions too. If you
want to mix both, you can use a `SplitLink` that can route links based on the request:

```cpp
#include <gqlxy/links/split_link.h>
#include <gqlxy/links/ws_link.h>

using namespace std;

Client client({
    .link = make_shared<SplitLink>(
        [](const GraphQLRequest& req) {
            return req.type != parser::OperationType::SUBSCRIPTION;
        },
        make_shared<HttpLink>(HttpLinkOptions{
            .url = "http://localhost:4000/graphql"
        }),
        make_shared<WsLink>(WsLinkOptions{
            .url  = "ws://localhost:4000/graphql"
        })
    ),
    .cache = make_shared<InMemoryCache>()
});
```
