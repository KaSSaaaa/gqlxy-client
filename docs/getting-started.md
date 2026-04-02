# Getting Started

## Installation

### Via vcpkg

Add to your `vcpkg.json`:

```json
{
  "dependencies": ["gqlxy-client"]
}
```

Then in your `CMakeLists.txt`:

```cmake
find_package(gqlxy-client CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE gqlxy::client)
```

### Building from Source

Prerequisites: CMake ≥ 3.10, Ninja, vcpkg on PATH.

```sh
cmake --preset arm64-osx-debug   # or x64-linux-debug, x64-windows-debug
cmake --build out/build/arm64-osx-debug
```

## Basic Usage

```cpp
#include <gqlxy/client.h>
#include <gqlxy/links/http_link.h>
#include <gqlxy/links/split_link.h>
#include <gqlxy/links/ws_link.h>
#include <gqlxy/cache/in_memory_cache.h>

using namespace gqlxy;

// Construct the client
Client client({
    .link = std::make_shared<SplitLink>(
        [](const GraphQLRequest& req) {
            return req.query.find("subscription") == std::string::npos;
        },
        std::make_shared<HttpLink>(HttpLinkOptions{.url = "http://localhost:4000/graphql"}),
        std::make_shared<WsLink>(WsLinkOptions{.url = "ws://localhost:4000/graphql"})
    ),
    .cache = std::make_shared<InMemoryCache>()
});

// Coroutine style
auto result = client.Query(R"( query { __typename } )").get();
if (result.data) { /* use result.data */ }

// RxCpp style
client.QueryRx(R"( query { __typename } )")
    .subscribe([](const GraphQLResult& r) { /* handle */ });

// Subscription
client.Subscribe(R"( subscription { onMessage { text } } )")
    .subscribe([](const GraphQLResult& r) { /* handle event */ });
```

## Running Tests

```sh
ctest --test-dir out/build/arm64-osx-debug --output-on-failure

# Single test
./out/build/arm64-osx-debug/tests/gqlxy_client_unit_tests --gtest_filter=SuiteName.TestName
```
