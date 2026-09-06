/*
 * main.c -- steak driver, lexer-slice bootstrap.
 *
 * Reads the file named by argv[1] (or stdin for "-"), lexes it through the
 * buffalo-generated DFA via buf_next(), and prints one line per token:
 *
 *     KIND "lexeme" line:col
 *
 * TOK_ERROR stops the run with a diagnostic on stderr and exit status 1.
 * The parser (buffalo %grammar) and the ASI filter pass (design.md #12)
 * slot in between the lexer loop and the printer later.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token_dump.h"

int main(int argc, char **argv)
{
    FILE *f = stdin;
    char *src;
    int len;
    BufLexer lx;

    if (argc > 2) {
        fprintf(stderr, "usage: steak [file|-]\n");
        return 2;
    }
    if (argc == 2 && strcmp(argv[1], "-") != 0) {
        f = fopen(argv[1], "rb");
        if (!f) {
            fprintf(stderr, "steak: cannot open %s\n", argv[1]);
            return 2;
        }
    }
    src = read_stream(f, &len);
    if (f != stdin) fclose(f);
    if (!src) {
        fprintf(stderr, "steak: out of memory\n");
        return 2;
    }

    buf_lexer_init(&lx, src, len);
    for (;;) {
        BufToken tok = buf_next(&lx);
        if (tok.kind == TOK_EOF) break;
        if (tok.kind == TOK_ERROR) {
            fprintf(stderr, "steak: unexpected character at %d:%d\n",
                    tok.line, tok.col);
            free(src);
            return 1;
        }
        print_token(stdout, tok);
    }

    free(src);
    return 0;
}
