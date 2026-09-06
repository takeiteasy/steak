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
// `check` lexes every example and diffs it against its .expected.

#include <stdio.h>

static const char *EXAMPLES[] = {"hello"};

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

    BuildTarget *check = RunCustom(ctx, "check", "echo 'steak: all examples ok'");

    for (int i = 0; i < (int)(sizeof(EXAMPLES) / sizeof(*EXAMPLES)); i++) {
        const char *name = EXAMPLES[i];
        char       src[128], cmd[512], checkname[64];
        snprintf(src, sizeof(src), "examples/%s.fn", name);
        snprintf(checkname, sizeof(checkname), "check-%s", name);
        snprintf(cmd, sizeof(cmd), "%s %s | diff -u examples/%s.expected -",
                 TargetOutput(steak), src, name);
        BuildTarget *c = RunCustom(ctx, checkname, cmd);
        DependsOn(c, steak);
        DependsOn(check, c);
    }

    return BuildDefault(ctx);
}
