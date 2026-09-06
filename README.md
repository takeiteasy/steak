# steak

steak is a small dynamic language with JS-flavoured syntax — closures, prototype delegation, method-call sugar, a wrapping numeric tower, try/catch/finally — implemented in C to the spec in `docs/design.md`. Built with [cccc](https://git.sr.ht/~takeiteasy/cccc), whose comptime pass runs [buffalo](https://git.sr.ht/~takeiteasy/buffalo) to lower `spec/steak.bflo` into the DFA tables linked into the binary: no `.bflo → .c` step, no checked-in generated code.

## Quick start

```sh
git submodule update --init   # vendor/buffalo
cccc --build build.c          # build/bin/steak + golden smoke
build/bin/steak examples/hello.fn   # token dump
```

`cccc` must be on `PATH` — it is the compiler, not just the build driver. Add `--build-cache` for incremental rebuilds (header dependencies are tracked automatically; only the `.bflo` read at comptime through `-D` needs a declared `AddInput`).

## Layout

- `spec/steak.bflo` — token vocabulary (buffalo spec); `spec/steak_tokens.h` is the matching checked-in token header the comptime pass validates
- `src/main.c` — driver: file → `buf_next()` → token dump (dump helpers live in `src/token_dump.h`)
- `tests/t_smoke.c` — golden smoke: `[[cccc::test]]` functions lex every `examples/*.fn` in-process and require the token dump to match its `.expected` byte for byte (replaces the old shell-diff loop; runs under `cccc --testing=vm`)
- `vendor/buffalo` — submodule; supplies the comptime pipeline (`src/buf_comptime.c`) and the lexer runtime (`runtime/buf_rt.c`)
- `build.c` — cccc build script: one `CcccExecutable` target (compiled by cccc itself via `--compile=native`, the only path that can run the comptime pass) plus the `check` target that re-execs the running cccc as `--testing=vm` over `tests/t_smoke.c`
- `docs/` — language design and roadmap (the spec this implementation follows)

Status: bootstrap — lexer slice only (keywords, identifiers, int/float/hex literals, plain strings, operators, comments, NEWLINE). Parser, ASI, and the rest of the pipeline are tracked in the ticket tracker.
