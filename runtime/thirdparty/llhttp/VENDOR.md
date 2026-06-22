# Vendored llhttp

These are the **pre-generated C release artifacts** of [nodejs/llhttp](https://github.com/nodejs/llhttp),
the maintained successor to the (now deprecated) Node `http-parser` this project used previously.

- **Upstream:** https://github.com/nodejs/llhttp
- **Version:** v9.4.2 (release tag `release/v9.4.2`)
- **License:** MIT (see `LICENSE-MIT`)

## Why the release branch and not the main branch?

llhttp's parser is described in TypeScript and compiled to C with `llparse`, which would require
Node/npm/npx in our build. The upstream **`release` branch** publishes the already-generated C sources
(`src/llhttp.c`, `src/api.c`, `src/http.c`, `include/llhttp.h`), so we vendor those directly and compile
them with our normal C toolchain — no JS tooling enters the build.

## Updating

To bump the version, copy these four files from the desired `release/vX.Y.Z` tag:

```
include/llhttp.h
src/api.c
src/http.c
src/llhttp.c
```

e.g. `curl -fsSL https://raw.githubusercontent.com/nodejs/llhttp/release/vX.Y.Z/<path> -o <path>`,
then update the version above.
