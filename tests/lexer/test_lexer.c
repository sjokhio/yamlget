/*
 * test_lexer.c — standalone driver for the yamlget lexer
 *
 * Reads a YAML file (or stdin), runs the lexer line by line, and prints each
 * classified line to stdout in pipe-separated format:
 *
 *   lineno|TYPE|indent|key|value
 *
 * Fields:
 *   lineno — 1-based source line number
 *   TYPE   — BLANK, COMMENT, KEY_ONLY, KEY_VALUE, INVALID, EOF
 *   indent — leading space count (0 for non-mapping lines and INVALID)
 *   key    — key name for KEY_ONLY/KEY_VALUE, empty otherwise
 *   value  — scalar value for KEY_VALUE, empty otherwise
 *
 * Stderr is used exclusively for lexer diagnostics. Stdout is clean output
 * suitable for comparison against .expected fixture files.
 *
 * Usage:
 *   test_lexer <file>   — classify lines from file
 *   test_lexer          — classify lines from stdin
 */

#include "yamlget/lexer.h"
#include "yamlget.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    FILE       *f;
    const char *source;

    if (argc == 1) {
        f      = stdin;
        source = "<stdin>";
    } else if (argc == 2) {
        f = fopen(argv[1], "r");
        if (!f) {
            fprintf(stderr, "test_lexer: cannot open '%s'\n", argv[1]);
            return YAMLGET_EXIT_FILE_ERROR;
        }
        source = argv[1];
    } else {
        fprintf(stderr, "Usage: test_lexer [file]\n");
        return YAMLGET_EXIT_BAD_ARGS;
    }

    yg_lexer_t lex;
    yg_line_t  line;
    yg_lexer_init(&lex, f, source);

    for (;;) {
        int rc = yg_lexer_next(&lex, &line);
        if (rc != 0) {
            fprintf(stderr, "test_lexer: fatal lexer error (rc=%d)\n", rc);
            if (f != stdin) fclose(f);
            return rc;
        }

        /* lineno|TYPE|indent|key|value */
        printf("%d|%s|%d|%s|%s\n",
               line.lineno,
               yg_line_type_name(line.type),
               line.indent,
               line.key,
               line.value);

        if (line.type == YG_LINE_EOF)
            break;
    }

    if (f != stdin) fclose(f);
    return 0;
}
