# Contributing to GQLXY Client

Thank you for your interest in contributing! This document explains how to get involved.

## Getting Started

1. **Fork** the repository on GitHub.
2. **Clone** your fork locally:
   ```sh
   git clone https://github.com/<your-username>/gqlxy-client.git
   cd gqlxy-client
   ```
3. **Create a branch** from `main` for your work:
   ```sh
   git checkout -b feat/my-feature
   ```
4. **Build** and make sure everything passes before you start:
   ```sh
   cmake --preset arm64-osx-debug
   cmake --build out/build/arm64-osx-debug
   ctest --test-dir out/build/arm64-osx-debug --output-on-failure
   ```

## Making Changes

### Coding Conventions

- **C++20** — the project requires `cxx_std_20`.
- **Public API in `include/gqlxy/`** — all public headers live there; internal implementation in `src/`.
- **Functions ≤ 20 lines** — keep functions concise and focused.
- **Prefer structs over multiple parameters** for better readability and extensibility (e.g. `HttpLinkOptions`).
- **`const&` by default** — pass arguments by const reference unless there is a reason not to.
- **`std::optional` over raw pointers** — no raw owning pointers.
- **Minimal comments** — code should be readable on its own. Only comment when something genuinely needs clarification.
- **DRY** — avoid code duplication.

### Formatting

The project uses `clang-format` (configured via `.clang-format` at the repo root).

Use `// clang-format off` / `// clang-format on` guards around deeply-nested initializer blocks if needed.

### Commit Style

Use [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short summary>

<optional body>
```

**Types:** `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `chore`, `ci`

**Examples:**
```
feat(http-link): implement POST query execution via boost::beast
fix(cache): fix race condition in InMemoryCache::Evict
docs(readme): update build instructions for Linux
test(split-link): cover predicate routing logic
```

- Keep the summary line under 72 characters.
- Use the imperative mood ("add support", not "added support").
- Reference issue numbers in the body when applicable (e.g., `Closes #42`).

## Submitting a Pull Request

1. **Push** your branch to your fork:
   ```sh
   git push origin feat/my-feature
   ```
2. **Open a Pull Request** against `main` on the upstream repository.
3. Fill out the PR template — it includes a checklist to help you.
4. Make sure CI passes (build + tests on all platforms).
5. A maintainer will review your PR. Be open to feedback and iterate if needed.

## Reporting Bugs & Requesting Features

Use the [issue templates](https://github.com/KaSSaaaa/gqlxy-client/issues/new/choose).

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
