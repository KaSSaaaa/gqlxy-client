---
title: Mutations
description: How to execute mutations with gqlxy-client
---

## Executing a Mutation

Mutations work the same way as queries:

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
    .variables = {
        {"title", "Hello"},
        {"body", "World"}
    }
});
```

## Updating Query results in the Cache

[//]: #TODO (Add documentation on how to update the cache)