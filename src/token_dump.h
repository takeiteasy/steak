/*
 * token_dump.h -- shared between the steak driver (main.c) and the golden
 * smoke (tests/t_smoke.c): the token-kind name table, the `KIND "lexeme"
 * line:col` dump line printer, and the whole-file reader. Kept in one place
 * so the driver and the smoke can never drift apart. All helpers are static
 * (header-only); each translation unit gets its own copy of the name table.
 */
#ifndef STEAK_TOKEN_DUMP_H
#define STEAK_TOKEN_DUMP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "buf_rt.h"
#include "steak_tokens.h"

static const char *tok_names[] = {
    "EOF", "ERROR",
    "KW_TRUE", "KW_FALSE", "KW_NULL",
    "KW_VAR", "KW_CONST", "KW_FUNCTION",
    "KW_IF", "KW_ELSE", "KW_WHILE", "KW_FOR", "KW_IN",
    "KW_RETURN", "KW_BREAK", "KW_CONTINUE",
    "KW_THROW", "KW_TRY", "KW_CATCH", "KW_FINALLY",
    "KW_AS", "KW_SYNTAX",
    "IDENT",
    "NEWLINE",
    "INT", "FLOAT", "STRING",
    "SEMI",
    "LPAREN", "RPAREN",
    "LBRACE", "RBRACE",
    "LBRACKET", "RBRACKET",
    "COMMA", "COLON", "DOT", "ELLIPSIS", "QUESTION",
    "PLUS", "MINUS", "STAR", "SLASH", "SLASHSLASH", "PERCENT",
    "INC", "DEC",
    "ASSIGN",
    "PLUSASSIGN", "MINUSASSIGN", "STARASSIGN", "SLASHASSIGN",
    "SLASHSLASHASSIGN", "PERCENTASSIGN",
    "AMPASSIGN", "PIPEASSIGN", "CARETASSIGN",
    "SHLASSIGN", "SHRASSIGN", "USHRASSIGN",
    "AMPAMPASSIGN", "PIPEPIPEASSIGN",
    "EQEQ", "NOTEQ", "LT", "LE", "GT", "GE",
    "SHL", "SHR", "USHR",
    "AMP", "PIPE", "CARET", "TILDE",
    "AMPAMP", "PIPEPIPE",
    "BANG"
};

#define TOK_NAME_COUNT (sizeof(tok_names) / sizeof(tok_names[0]))

static const char *tok_name(int kind)
{
    if (kind < 0 || (size_t)kind >= TOK_NAME_COUNT) return "?";
    return tok_names[kind];
}

/* Print a lexeme (not NUL-terminated) with \n \t \r \\ and " escaped. */
static void print_lexeme(FILE *out, const char *lexeme, int length)
{
    int i;
    fputc('"', out);
    for (i = 0; i < length; i++) {
        switch (lexeme[i]) {
        case '\n': fputs("\\n", out); break;
        case '\t': fputs("\\t", out); break;
        case '\r': fputs("\\r", out); break;
        case '\\': fputs("\\\\", out); break;
        case '"':  fputs("\\\"", out); break;
        default:   fputc(lexeme[i], out); break;
        }
    }
    fputc('"', out);
}

/* Print one dump line: KIND "lexeme" line:col */
static void print_token(FILE *out, BufToken tok)
{
    fprintf(out, "%s ", tok_name(tok.kind));
    print_lexeme(out, tok.lexeme, tok.length);
    fprintf(out, " %d:%d\n", tok.line, tok.col);
}

/* Read a whole stream into a malloc'd NUL-terminated buffer; the lexer only
 * needs [src, src+len) but NUL costs one byte. */
static char *read_stream(FILE *f, int *out_len)
{
    size_t cap = 1 << 16, len = 0, n;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    while ((n = fread(buf + len, 1, cap - len - 1, f)) > 0) {
        len += n;
        if (cap - len < 2) {
            char *tmp = realloc(buf, cap * 2);
            if (!tmp) { free(buf); return NULL; }
            buf = tmp;
            cap *= 2;
        }
    }
    buf[len] = '\0';
    *out_len = (int)len;
    return buf;
}

/* Read a whole file into a malloc'd NUL-terminated buffer; NULL on I/O or
 * memory failure (caller reports the reason). */
static char *read_file(const char *path, int *out_len)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    if (!f) return NULL;
    buf = read_stream(f, out_len);
    fclose(f);
    return buf;
}

#endif /* STEAK_TOKEN_DUMP_H */
