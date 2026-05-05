---
title: Queries
description: How to execute queries with gqlxy-client
---

## Executing your first Query

With gqlxy-client, you can execute your queries in 2 ways:

### C++20 Coroutine
You can use coroutines to `co_await` on your request:

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

### RxCpp
`Observable<T>` are `rxcpp::observable<T>` internally. Meaning that you can also subscribe to your queries and get notified
when the request is successful or not:

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

## Caching
By default, all the queries will be cached in the client's `InMemoryCache` unless you specify another `FetchPolicy`.
See more on caching [here](../caching/cache.md).