/*
 * yamlget — dependency-free YAML key extractor
 *
 * Usage: yamlget <file> <dot.path>
 *        yamlget - <dot.path>      (read from stdin)
 *        yamlget --version
 *        yamlget --help
 */

#include "yamlget.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROG "yamlget"

static void print_usage(FILE *out)
{
    fprintf(out,
        "Usage: " PROG " <file> <path>\n"
        "       " PROG " - <path>       (read from stdin)\n"
        "\n"
        "Extract a scalar value from a YAML file by dot-notation path.\n"
        "\n"
        "Arguments:\n"
        "  <file>   Path to a YAML file, or '-' to read from stdin.\n"
        "  <path>   Dot-notation key path (e.g. 'server.port', 'app.name').\n"
        "\n"
        "Exit codes:\n"
        "  0  Success\n"
        "  1  Key not found\n"
        "  2  Invalid arguments\n"
        "  3  File error\n"
        "  4  Parse error\n"
        "  5  Internal error\n"
        "\n"
        "Version: " YAMLGET_VERSION_STRING "\n"
    );
}

int main(int argc, char *argv[])
{
    if (argc == 2) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0) {
            printf(YAMLGET_VERSION_STRING "\n");
            return YAMLGET_EXIT_OK;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_usage(stdout);
            return YAMLGET_EXIT_OK;
        }
    }

    if (argc != 3) {
        fprintf(stderr, PROG ": expected 2 arguments, got %d\n", argc - 1);
        fprintf(stderr, "Run '" PROG " --help' for usage.\n");
        return YAMLGET_EXIT_BAD_ARGS;
    }

    const char *filepath = argv[1];
    const char *keypath  = argv[2];

    if (keypath[0] == '\0') {
        fprintf(stderr, PROG ": key path must not be empty\n");
        return YAMLGET_EXIT_BAD_ARGS;
    }

    /*
     * Parser and lookup are not yet implemented.
     * The stubs below represent the call sites that will be filled in
     * during M2 (lexer), M3 (parser), and M4 (lookup).
     */

    (void)filepath; /* suppress unused-variable warning until parser lands */

    fprintf(stderr, PROG ": parser not yet implemented\n");
    return YAMLGET_EXIT_INTERNAL;
}
