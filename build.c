// Build script for steak. Run from the repository root with:
//
//     cccc --build build.c                    # build + golden smoke
//     cccc --build build.c --build-target=steak
//     cccc --build build.c --build-cache      # incremental; header deps tracked
//
// cccc is the compiler: the comptime pass in vendor/buffalo/src/buf_comptime.c
// reads spec/steak.bflo and lowers it to the DFA tables + buf_next() wrapper
// linked into the binary -- there is no .bflo -> .c step and no checked-in
// generated code. A CcccExecutable target is compiled by the running cccc
// itself (one whole-program `cccc --compile=native` invocation), the only
// path that can run that comptime pass; a plain Executable's host cc cannot.
// That constraint is why steak previously needed a Makefile driving a
// one-shot `cccc -c=native` line by hand.
//
// `check` is the golden smoke (ticket #7): cccc's built-in test runner picks
// it up as a --testing=vm run of tests/t_smoke.c, which lexes every example
// in-process and requires its token dump to match examples/*.expected
// byte for byte. The comptime pass runs again inside that invocation (the
// .bflo is handed over through -D BUF_SPEC), so the smoke needs no built
// artifact and no shell diffing -- it just re-execs the running cccc.

#include <stdio.h>

[[cccc::build]]
int build_main(Builder *ctx) {
    BuildTarget *steak = CcccExecutable(ctx, "steak");
    AddSource(steak, "src/main.c");
    AddSource(steak, "vendor/buffalo/src/buf_comptime.c");
    AddSource(steak, "vendor/buffalo/runtime/buf_rt.c");
    AddInclude(steak, "vendor/buffalo/include/buffalo");
    AddInclude(steak, "vendor/buffalo/src");
    AddInclude(steak, "vendor/buffalo/runtime");
    AddInclude(steak, "spec");
    AddDefine(steak, "BUF_SPEC", "\"spec/steak.bflo\"");
    AddDefine(steak, "BUF_STOP_AFTER", "5");

    // The .bflo is read at comptime through -D, not #included, so no depfile
    // ever sees it: declare it so a grammar edit invalidates the cached
    // binary. Everything else (token header, runtime, comptime modules) is a
    // real #include and lands in cccc's --deps-file tracking on its own.
    AddInput(steak, "spec/steak.bflo");

    // The token header is a generated artifact, not checked in: regenerated
    // from the spec's %tokens list by buffalo's token-header generator
    // (plain cc, no cccc -- vendor/buffalo/bin/buffalo builds its buf_tokgen
    // binary on demand the first time). The comptime pass still validates it
    // like any hand-written header; this rule just removes the
    // hand-maintenance. Declared inputs drive the up-to-date skip, so a
    // %tokens edit regenerates the header and a header change recompiles
    // the binary.
    BuildTarget *gen_tokens = RunCustom(
        ctx, "gen-tokens",
        "vendor/buffalo/bin/buffalo tokens spec/steak.bflo"
        " -o spec/steak_tokens.h");
    DeclareOutput(gen_tokens, "spec/steak_tokens.h");
    AddInput(gen_tokens, "spec/steak.bflo");
    AddInput(gen_tokens, "vendor/buffalo/src/buf_rx.c");
    AddInput(gen_tokens, "vendor/buffalo/src/buf_tokgen.c");
    AddInput(gen_tokens, "vendor/buffalo/tools/buf_tokgen_main.c");
    AddInput(gen_tokens, "vendor/buffalo/include/buffalo/buf_rx.h");
    AddInput(gen_tokens, "vendor/buffalo/include/buffalo/buf_tokgen.h");
    DependsOn(steak, gen_tokens);

    // Golden smoke: re-run this cccc over the test suite (same comptime pass
    // and flags as the steak target above) instead of shelling out to diff.
    // RunCustom with no declared output means the smoke runs on every build.
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "'%s' --testing=vm -D BUF_SPEC='\"spec/steak.bflo\"'"
             " -D BUF_STOP_AFTER=5"
             " -Ispec -Isrc -Ivendor/buffalo/include/buffalo"
             " -Ivendor/buffalo/src -Ivendor/buffalo/runtime"
             " tests/t_smoke.c"
             " vendor/buffalo/src/buf_comptime.c"
             " vendor/buffalo/runtime/buf_rt.c",
             CcccPath(ctx));
    BuildTarget *check = RunCustom(ctx, "check", cmd);
    AddInput(check, "spec/steak.bflo");
    DependsOn(check, steak);

    return BuildDefault(ctx);
}
