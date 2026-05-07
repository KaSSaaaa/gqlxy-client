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

Because `InMemoryCache` is normalized, mutations that return known entities automatically update all queries that
reference those entities.

For example, if a `GetPost` query already has `Post:42` in the cache and a mutation returns a `Post` with the same `id`,
the cache entry is merged in place and any `CacheFirst` or `CacheAndNetwork` query that reads that post will see
the updated data immediately.

```cpp
auto result = co_await client.Mutation({
    .query = R"(
        mutation UpdatePost($id: ID!, $title: String!) {
            updatePost(id: $id, title: $title) {
                id
                title
            }
        }
    )",
    .variables = {
        {"id", "42"},
        {"title", "Updated title"}
    }
});
```

If the mutation returns `{ id: "42", title: "Updated title" }`, the cache entry for `Post:42` is updated and every
active query that depends on that entity reflects the change on its next read.

To force queries to re-fetch after a mutation that does not return enough normalized data (e.g. it only returns a count), use `Refetch()`:

```cpp
co_await client.Refetch({
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

When you need full control you can call `Write` directly on the cache to inject or replace a query result, and `Read` to
retrieve the current cached value for a query:

```cpp
GraphQLRequest postsRequest{
    .query = R"(
        query {
            posts {
                id
                title
            }
        }
    )"
};

// Read the current cached result
auto cached = cache->Read(postsRequest);

if (cached && cached->data) {
    // Append the newly created post returned by the mutation
    auto updated = *cached->data;
    updated["posts"].push_back(result.data.value()["createPost"]);

    // Write the modified result back into the cache
    cache->Write(postsRequest, GraphQLResponse{.data = updated});
}
```

`Write` normalizes the response through the same entity store as a network result, so any overlapping entities are merged automatically.