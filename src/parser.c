/*
 * parser.c — streaming dot-path resolver
 *
 * See include/yamlget/parser.h for design notes and the public API.
 *
 * Algorithm overview
 * ──────────────────
 * We maintain a small stack of (indent, matched) frames. Each frame records
 * the indentation of a mapping key we have entered and whether that key was
 * on our target path.
 *
 * For every lexer line:
 *
 *   1. Pop frames whose indent >= current line's indent, decrementing
 *      match_depth for any popped frame that was "matched".
 *
 *   2. Determine eligibility: a line may be compared against the next path
 *      segment only when the stack is empty or the top frame is "matched".
 *      If the top frame is "unmatched" (we descended into a sibling branch)
 *      the current line is inside that sibling and must be skipped.
 *
 *   3. If eligible and the key equals path[match_depth]:
 *        - Increment match_depth.
 *        - If match_depth == seg_count we found the target: print and exit.
 *        - Otherwise push a "matched" frame and continue.
 *
 *   4. If not eligible or key mismatch:
 *        - Push an "unmatched" frame so deeper lines are blocked.
 *
 * This approach is O(n) in the file length, uses O(depth) stack space, and
 * performs zero dynamic allocation.
 */

#include "yamlget/parser.h"
#include "yamlget/lexer.h"
#include "yamlget.h"

#include <stdio.h>
#include <string.h>

/* Maximum YAML nesting depth the stack can represent. */
#define YG_PARSER_STACK_MAX 64

/* One entry in the indentation stack. */
typedef struct {
    int indent;   /* leading spaces of the key on this level */
    int matched;  /* 1 if this level's key was on the lookup path */
} frame_t;

/* ── Path splitting ───────────────────────────────────────────────────────── */

int yg_path_split(const char *path, char segs[][YG_KEY_MAX], int max)
{
    if (!path || path[0] == '\0') return -1;

    int   n = 0;
    const char *p = path;

    while (*p && n < max) {
        const char *dot = strchr(p, '.');
        size_t      len = dot ? (size_t)(dot - p) : strlen(p);

        if (len == 0 || len >= (size_t)YG_KEY_MAX)
            return -1; /* empty segment or segment too long */

        memcpy(segs[n], p, len);
        segs[n][len] = '\0';
        n++;

        if (!dot) break;
        p = dot + 1;

        if (*p == '\0') return -1; /* trailing dot */
    }

    return (n > 0) ? n : -1;
}

/* ── Streaming lookup ─────────────────────────────────────────────────────── */

int yg_stream_lookup(FILE *stream, const char *source,
                     char segs[][YG_KEY_MAX], int seg_count)
{
    yg_lexer_t lex;
    yg_line_t  line;
    yg_lexer_init(&lex, stream, source);

    frame_t stack[YG_PARSER_STACK_MAX];
    int     top         = -1; /* index of topmost frame; -1 = empty */
    int     match_depth =  0; /* segments matched so far            */

    for (;;) {
        int rc = yg_lexer_next(&lex, &line);
        if (rc != 0)
            return YAMLGET_EXIT_FILE_ERROR;

        switch (line.type) {
            case YG_LINE_EOF:
                return YAMLGET_EXIT_NOT_FOUND;

            case YG_LINE_BLANK:
            case YG_LINE_COMMENT:
                continue;

            case YG_LINE_INVALID:
                /* Lexer already printed the diagnostic. */
                return YAMLGET_EXIT_PARSE_ERROR;

            case YG_LINE_KEY_ONLY:
            case YG_LINE_KEY_VALUE:
                break; /* fall through to resolution logic */
        }

        /* ── Step 1: pop frames at the same or deeper indent ────────────── */
        while (top >= 0 && stack[top].indent >= line.indent) {
            if (stack[top].matched)
                match_depth--;
            top--;
        }

        /* ── Step 2: eligibility check ───────────────────────────────────── */
        /*
         * A line is only eligible to be compared against the next path
         * segment when either:
         *   (a) the stack is empty (we are at root level), or
         *   (b) the top frame is a "matched" frame (we descended on-path).
         *
         * If the top is "unmatched" we are inside a sibling branch that
         * diverged from our target path, and we must skip this line.
         */
        int eligible = (top < 0 || stack[top].matched);

        if (eligible &&
            match_depth < seg_count &&
            strcmp(line.key, segs[match_depth]) == 0) {

            /* ── Step 3: key matches the next path segment ─────────────── */
            match_depth++;

            if (match_depth == seg_count) {
                /*
                 * Full path matched.
                 * KEY_VALUE → print the scalar.
                 * KEY_ONLY  → the value is empty; print an empty line so
                 *             the exit code (0 = found) is unambiguous.
                 */
                if (line.type == YG_LINE_KEY_VALUE) {
                    printf("%s\n", line.value);
                } else {
                    printf("\n");
                }
                return YAMLGET_EXIT_OK;
            }

            /* Intermediate segment matched; descend into this mapping. */
            if (top + 1 >= YG_PARSER_STACK_MAX) {
                fprintf(stderr, "%s:%d: nesting exceeds maximum depth (%d)\n",
                        source, line.lineno, YG_PARSER_STACK_MAX);
                return YAMLGET_EXIT_INTERNAL;
            }
            top++;
            stack[top].indent  = line.indent;
            stack[top].matched = 1;

        } else {
            /* ── Step 4: key does not match — push unmatched frame ──────── */
            /*
             * We must push even on mismatch so that any deeper keys (which
             * belong to this sibling branch) are correctly blocked from being
             * tested against our path segments.
             */
            if (top + 1 >= YG_PARSER_STACK_MAX) {
                fprintf(stderr, "%s:%d: nesting exceeds maximum depth (%d)\n",
                        source, line.lineno, YG_PARSER_STACK_MAX);
                return YAMLGET_EXIT_INTERNAL;
            }
            top++;
            stack[top].indent  = line.indent;
            stack[top].matched = 0;
        }
    }
}
