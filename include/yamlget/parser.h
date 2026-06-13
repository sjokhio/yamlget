#ifndef YAMLGET_PARSER_H
#define YAMLGET_PARSER_H

/*
 * yamlget parser — streaming dot-path resolver
 *
 * Sits on top of the lexer (yamlget/lexer.h) and resolves a single
 * dot-notation lookup path against a YAML mapping stream.
 *
 * Design constraints (same as the lexer layer):
 *   - Pure C99, no external dependencies.
 *   - No dynamic memory allocation.
 *   - No AST — single streaming pass, minimum state.
 *   - No global state; all state is stack-allocated inside yg_stream_lookup.
 *
 * Supported YAML subset in v0.1.0:
 *   - Block mappings, arbitrarily nested (indent-based).
 *   - Scalar values: plain, single-quoted, double-quoted.
 *   - Block scalars: literal (|) and folded (>), all chomping indicators.
 *   - Blank lines and comments (ignored).
 *   - CRLF and LF line endings.
 *
 * Explicitly unsupported (returns YAMLGET_EXIT_PARSE_ERROR):
 *   - Sequences / arrays (- item).
 *   - Any line the lexer cannot classify as a mapping entry.
 *
 * Silently unsupported (not present in the lookup path — ignored):
 *   - Anchors, aliases, tags, merge keys.
 *     These appear as ordinary KEY_VALUE lines to the lexer; if they are
 *     not on the active lookup path they are simply not followed.
 */

#include "yamlget/lexer.h"   /* for YG_KEY_MAX */
#include <stdio.h>

/* Maximum number of dot-separated segments in a lookup path. */
#define YG_PATH_MAX_DEPTH 32

/*
 * yg_path_split — split a dot-notation path into segments.
 *
 * Writes up to `max` segments into `segs[0..max-1]`. Each segment is a
 * null-terminated string at most YG_KEY_MAX-1 bytes long.
 *
 * Returns the number of segments on success (>= 1).
 * Returns -1 if the path is empty, has an empty segment (leading, trailing,
 * or consecutive dots), or a segment that exceeds YG_KEY_MAX-1 bytes.
 */
int yg_path_split(const char *path, char segs[][YG_KEY_MAX], int max);

/*
 * yg_stream_lookup — resolve a dot-path against a YAML stream.
 *
 * Reads from `stream` one line at a time, maintaining an indentation stack
 * to track the current mapping hierarchy. When the full `path` is matched,
 * prints the scalar value to stdout and returns YAMLGET_EXIT_OK.
 *
 * `source` is used verbatim in error messages (typically the filename or
 * "<stdin>"). `segs` and `seg_count` are the result of yg_path_split().
 *
 * Return values:
 *   YAMLGET_EXIT_OK          — value found and printed
 *   YAMLGET_EXIT_NOT_FOUND   — path not found in document
 *   YAMLGET_EXIT_PARSE_ERROR — lexer reported an invalid line
 *   YAMLGET_EXIT_FILE_ERROR  — I/O error reading the stream
 *   YAMLGET_EXIT_INTERNAL    — nesting depth exceeded YG_PARSER_STACK_MAX
 */
int yg_stream_lookup(FILE *stream, const char *source,
                     char segs[][YG_KEY_MAX], int seg_count);

#endif /* YAMLGET_PARSER_H */
