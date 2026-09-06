/*
 * t_smoke.c -- golden smoke for the lexer slice, driven through cccc's built
 * in test framework ([[cccc::test]]; ticket #7's evaluation of --testing
 * against the comptime pass came out positive on both backends, so the
 * smoke runs in-process under `cccc --testing=vm` instead of shelling out to
 * diff: build.c's `check` target invokes it with the same -D/-I flags the
 * steak target is compiled with).
 *
 * One test per entry in EXAMPLES: lex examples/<name>.fn through buf_next(),
 * render the same `KIND "lexeme" line:col` dump the driver prints, and
 * require it to match examples/<name>.expected byte for byte. The .expected
 * files are read at test time (not embedded) so they stay the single source
 * of truth.
 *
 * Run via `cccc --build build.c` (the check target) or directly:
 *
 *     cccc --testing=vm -D BUF_SPEC='"spec/steak.bflo"' -D BUF_STOP_AFTER=5 \
 *         -Ispec -Isrc -Ivendor/buffalo/include/buffalo \
 *         -Ivendor/buffalo/src -Ivendor/buffalo/runtime \
 *         tests/t_smoke.c vendor/buffalo/src/buf_comptime.c \
 *         vendor/buffalo/runtime/buf_rt.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token_dump.h"

static const char *EXAMPLES[] = {"hello"};

/* Growable output buffer appending a printf-style formatting subset by hand
 * (kind name, escaped lexeme, ints) so the dump needs no FILE* and prints
 * nothing on a passing run. */
typedef struct {
    char  *data;
    size_t len, cap;
} StrBuf;

static void sb_grow(StrBuf *sb, size_t need)
{
    size_t cap = sb->cap ? sb->cap : 1 << 12;
    char *tmp;
    while (cap - sb->len < need) cap *= 2;
    if (cap == sb->cap) return;
    tmp = realloc(sb->data, cap);
    if (!tmp) {
        fprintf(stderr, "t_smoke: out of memory\n");
        exit(2);
    }
    sb->data = tmp;
    sb->cap = cap;
}

static void sb_puts(StrBuf *sb, const char *s)
{
    size_t n = strlen(s);
    sb_grow(sb, n + 1);
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
}

static void sb_putc(StrBuf *sb, char c)
{
    sb_grow(sb, 2);
    sb->data[sb->len++] = c;
    sb->data[sb->len] = '\0';
}

static void sb_putint(StrBuf *sb, int v)
{
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", v);
    sb_puts(sb, tmp);
}

/* Same escaping as the driver's lexeme printer. */
static void sb_putlexeme(StrBuf *sb, const char *lexeme, int length)
{
    int i;
    sb_putc(sb, '"');
    for (i = 0; i < length; i++) {
        switch (lexeme[i]) {
        case '\n': sb_puts(sb, "\\n"); break;
        case '\t': sb_puts(sb, "\\t"); break;
        case '\r': sb_puts(sb, "\\r"); break;
        case '\\': sb_puts(sb, "\\\\"); break;
        case '"':  sb_puts(sb, "\\\""); break;
        default:   sb_putc(sb, lexeme[i]); break;
        }
    }
    sb_putc(sb, '"');
}

/* Line "head" of s: start of line containing offset at, and one past its end */
static void line_bounds(const char *s, size_t at, size_t *b, size_t *e)
{
    size_t i = at > strlen(s) ? strlen(s) : at;
    while (i > 0 && s[i - 1] != '\n') i--;
    *b = i;
    for (*e = i; s[*e] && s[*e] != '\n'; (*e)++)
        ;
}

static void run_smoke(const char *name)
{
    char src_path[64], exp_path[64];
    int src_len = 0, exp_len = 0;
    char *src, *expected;
    StrBuf got = {0};
    BufLexer lx;

    snprintf(src_path, sizeof(src_path), "examples/%s.fn", name);
    snprintf(exp_path, sizeof(exp_path), "examples/%s.expected", name);

    src = read_file(src_path, &src_len);
    if (!src) {
        printf("t_smoke: cannot read %s\n", src_path);
        AssertFail();
        return;
    }
    expected = read_file(exp_path, &exp_len);
    if (!expected) {
        printf("t_smoke: cannot read %s\n", exp_path);
        free(src);
        AssertFail();
        return;
    }

    buf_lexer_init(&lx, src, src_len);
    for (;;) {
        BufToken tok = buf_next(&lx);
        if (tok.kind == TOK_EOF) break;
        if (tok.kind == TOK_ERROR) {
            printf("t_smoke: %s lexed TOK_ERROR at %d:%d\n",
                   src_path, tok.line, tok.col);
            free(src);
            free(expected);
            free(got.data);
            AssertFail();
            return;
        }
        sb_puts(&got, tok_name(tok.kind));
        sb_putc(&got, ' ');
        sb_putlexeme(&got, tok.lexeme, tok.length);
        sb_putc(&got, ' ');
        sb_putint(&got, tok.line);
        sb_putc(&got, ':');
        sb_putint(&got, tok.col);
        sb_putc(&got, '\n');
    }

    if (strcmp(got.data ? got.data : "", expected) != 0) {
        const char *a = got.data ? got.data : "";
        const char *b = expected;
        size_t i = 0, ab, ae, bb, be;
        while (a[i] == b[i] && a[i] && b[i]) i++;
        line_bounds(a, i, &ab, &ae);
        line_bounds(b, i, &bb, &be);
        printf("t_smoke: %s output mismatch at first differing line\n"
               "  got:      %.*s\n"
               "  expected: %.*s\n",
               exp_path, (int)(ae - ab), a + ab, (int)(be - bb), b + bb);
        AssertFailMsg("token dump does not match expected (see above)");
    }

    free(src);
    free(expected);
    free(got.data);
}

/* One registration per example; keep in sync with EXAMPLES above. */
[[cccc::test(name = "smoke: hello")]]
void smoke_hello(void) { run_smoke("hello"); }
