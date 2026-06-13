/*
 * yamlget — dependency-free YAML key extractor
 *
 * Usage: yamlget <file> <dot.path>
 *        yamlget - <dot.path>      (read from stdin)
 *        yamlget --version
 *        yamlget --help
 */

#include "yamlget.h"
#include "yamlget/lexer.h"
#include "yamlget/parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROG "yamlget"

static void print_usage(FILE *out)
{
    fprintf(out,
        "Usage: " PROG " <file> <path>\n"
        "       " PROG " - <path>         read from stdin\n"
        "       " PROG " --version\n"
        "       " PROG " --help\n"
        "\n"
        "Extract a scalar value from a YAML mapping by dot-notation path.\n"
        "The value is printed to stdout followed by a newline. All diagnostics\n"
        "go to stderr so stdout is safe to capture with $() or pipes.\n"
        "\n"
        "Arguments:\n"
        "  <file>   YAML file to read, or '-' for stdin.\n"
        "  <path>   Dot-notation key path (e.g. 'app.name', 'db.port').\n"
        "           Segments are separated by '.'. Segments may not be empty.\n"
        "\n"
        "Examples:\n"
        "  " PROG " config.yaml database.host\n"
        "  " PROG " config.yaml deploy.image.tag\n"
        "  " PROG " Chart.yaml appVersion\n"
        "  VERSION=$(" PROG " config.yaml app.version)\n"
        "  cat config.yaml | " PROG " - app.name\n"
        "\n"
        "Exit codes:\n"
        "  0  Key found — value printed to stdout\n"
        "  1  Key not found\n"
        "  2  Bad arguments (wrong count, malformed path)\n"
        "  3  File error (not found, not readable)\n"
        "  4  Parse error (unsupported or invalid YAML)\n"
        "  5  Internal error (always a bug — please report)\n"
        "\n"
        "Supported YAML:\n"
        "  Block mappings (nested, any depth), plain/single-quoted/double-quoted\n"
        "  scalars, literal block scalars (|) and folded block scalars (>),\n"
        "  all chomping indicators (-, +), LF and CRLF line endings.\n"
        "\n"
        "Not supported (exits 4): sequences (- item), multi-document (---),\n"
        "  tab-indented files.\n"
        "\n"
        "Version: " YAMLGET_VERSION_STRING
        "  <https://github.com/your-org/yamlget>\n"
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

    /* ── Split the dot-notation path ──────────────────────────────────────── */

    char segs[YG_PATH_MAX_DEPTH][YG_KEY_MAX];
    int  seg_count = yg_path_split(keypath, segs, YG_PATH_MAX_DEPTH);

    if (seg_count < 0) {
        fprintf(stderr, PROG ": invalid key path '%s' "
                "(empty segment, trailing dot, or segment too long)\n", keypath);
        return YAMLGET_EXIT_BAD_ARGS;
    }

    /* ── Open input ───────────────────────────────────────────────────────── */

    FILE       *f;
    const char *source;

    if (strcmp(filepath, "-") == 0) {
        f      = stdin;
        source = "<stdin>";
    } else {
        f = fopen(filepath, "r");
        if (!f) {
            fprintf(stderr, PROG ": cannot open '%s': %s\n",
                    filepath, strerror(errno));
            return YAMLGET_EXIT_FILE_ERROR;
        }
        source = filepath;
    }

    /* ── Stream lookup ────────────────────────────────────────────────────── */

    int rc = yg_stream_lookup(f, source, segs, seg_count);

    if (f != stdin)
        fclose(f);

    if (rc == YAMLGET_EXIT_NOT_FOUND) {
        fprintf(stderr, PROG ": key not found: '%s'\n", keypath);
    }

    return rc;
}
