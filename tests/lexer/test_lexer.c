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
 * Embedded newlines in block scalar values are printed as the two-character
 * sequence \n (backslash + 'n') so that each logical record occupies exactly
 * one output line. Literal backslashes are doubled (\\).
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

#ifdef _WIN32
# include <fcntl.h>
# include <io.h>
#endif

static void print_escaped(const char *s)
{
    for (; *s; s++) {
        if (*s == '\n')      { fputs("\\n", stdout); }
        else if (*s == '\\') { fputs("\\\\", stdout); }
        else                 { fputc(*s, stdout); }
    }
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif

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

        /* lineno|TYPE|indent|key|value  (value has \n and \\ escaped) */
        printf("%d|%s|%d|%s|",
               line.lineno,
               yg_line_type_name(line.type),
               line.indent,
               line.key);
        print_escaped(line.value);
        putchar('\n');

        if (line.type == YG_LINE_EOF)
            break;
    }

    if (f != stdin) fclose(f);
    return 0;
}
