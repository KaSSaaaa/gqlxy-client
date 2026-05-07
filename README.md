# gqlxy-client

```cpp
gqlxy::Client client({
    .link = make_shared<SplitLink>(...),
    .cache = make_shared<InMemoryCache>()
});

auto result = co_await client.Query(R"()");

if (result.data) {
    ...
} else if (result.errors) {
    ...
}
```