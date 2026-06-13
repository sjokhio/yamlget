#ifndef YAMLGET_LEXER_H
#define YAMLGET_LEXER_H

/*
 * yamlget lexer — line-oriented YAML classifier
 *
 * Reads a YAML input stream one line at a time and classifies each line into
 * one of a small set of structural types. Tracks indentation depth and
 * extracts key names and scalar values for mapping entries.
 *
 * This module is the foundation for the streaming parser (M3). It has no
 * knowledge of document structure, indentation rules, or path resolution —
 * those concerns belong to later layers.
 *
 * Design constraints:
 *   - Pure C99, no external dependencies.
 *   - No dynamic memory allocation.
 *   - No global state — all state lives in yg_lexer_t.
 *   - Fail-fast: errors are reported immediately to stderr with filename and
 *     line number; the offending line is classified as YG_LINE_INVALID.
 *
 * Limitations in v0.1.0:
 *   - Block scalars (| and >) are fully supported: literal, folded, and all
 *     three chomping indicators (clip/strip/keep). Values are assembled into
 *     yg_line_t.value with embedded '\n' separators where appropriate.
 *   - Quoted keys are accepted but returned verbatim (quotes included).
 *   - Sequence items (- ...) are classified as YG_LINE_INVALID.
 *   - Anchors, aliases, and tags are not recognised.
 */

#include <stdio.h>

/* ── Sizing constants ─────────────────────────────────────────────────────── */

#define YG_KEY_MAX    256   /* max bytes in a key name (null terminator incl.) */
#define YG_VALUE_MAX 4096   /* max bytes in a scalar value (null term. incl.)  */

/*
 * Internal line buffer. Large enough for the longest realistic YAML line plus
 * both field maxima. Lines that exceed this are read partially; the resulting
 * classification will be INVALID or incorrect, which is acceptable — YAML
 * lines of this length do not appear in real CI/CD configuration files.
 */
#define YG_LINE_BUF_MAX (YG_KEY_MAX + YG_VALUE_MAX + 32)

/* ── Line classification ──────────────────────────────────────────────────── */

/*
 * Structural classification of one logical YAML line.
 *
 * Only two types carry structured data (key / value):
 *   YG_LINE_KEY_ONLY  — key is set; value is empty; has_value == 0.
 *   YG_LINE_KEY_VALUE — key and value are both set; has_value == 1.
 *
 * All other types have key == "" and value == "".
 */
typedef enum {
    YG_LINE_BLANK,      /* empty or whitespace-only                        */
    YG_LINE_COMMENT,    /* first non-space character is '#'                */
    YG_LINE_KEY_ONLY,   /* mapping entry with no scalar: "key:"            */
    YG_LINE_KEY_VALUE,  /* mapping entry with scalar:   "key: value"       */
    YG_LINE_INVALID,    /* line that cannot be classified; error on stderr  */
    YG_LINE_EOF         /* end of input stream; no further lines available  */
} yg_line_type_t;

/* ── Lexer record ─────────────────────────────────────────────────────────── */

/*
 * Structured result of lexing one line. All fields are populated by
 * yg_lexer_next(); callers must not modify them.
 *
 * String fields are null-terminated C strings owned by this struct. They are
 * valid until the next call to yg_lexer_next() on the same lexer.
 */
typedef struct {
    yg_line_type_t  type;
    int             lineno;              /* 1-based source line number         */
    int             indent;             /* leading space count (0 for INVALID) */
    char            key[YG_KEY_MAX];    /* key name (KEY_ONLY / KEY_VALUE)    */
    char            value[YG_VALUE_MAX];/* scalar value (KEY_VALUE only)      */
    int             has_value;          /* 1 = KEY_VALUE, 0 = KEY_ONLY / other */
} yg_line_t;

/* ── Lexer state ──────────────────────────────────────────────────────────── */

/*
 * Treat this struct as opaque outside of lexer.c.
 * Initialise with yg_lexer_init(); do not copy or compare.
 */
typedef struct {
    FILE       *stream;              /* input stream (not owned)              */
    const char *source;             /* filename for diagnostics              */
    int         lineno;             /* lines consumed so far (0 = start)     */
    char        buf[YG_LINE_BUF_MAX];
    int         buf_pending;        /* 1 if buf holds an unprocessed lookahead line */
} yg_lexer_t;

/* ── Public API ───────────────────────────────────────────────────────────── */

/*
 * yg_lexer_init — initialise a lexer.
 *
 * `stream` must be open for reading; the lexer does not close it.
 * `source` is used verbatim in error messages (typically the filename or
 * "<stdin>"). If NULL, "<unknown>" is substituted.
 *
 * The lexer holds a pointer to `source` — the caller must keep the string
 * alive for the lexer's lifetime.
 */
void yg_lexer_init(yg_lexer_t *lex, FILE *stream, const char *source);

/*
 * yg_lexer_next — read and classify the next line.
 *
 * Populates `out` and returns 0 on success, including normal EOF (in which
 * case out->type == YG_LINE_EOF). Returns YAMLGET_EXIT_FILE_ERROR only on a
 * hard I/O error that prevents further reading.
 *
 * Error lines (tab indentation, missing colon, empty key, value too long)
 * are reported to stderr and returned as YG_LINE_INVALID with exit code 0 —
 * the caller decides whether to abort or continue.
 */
int yg_lexer_next(yg_lexer_t *lex, yg_line_t *out);

/*
 * yg_line_type_name — return a short, stable ASCII label for a line type.
 * Never returns NULL. Suitable for logging and test output.
 */
const char *yg_line_type_name(yg_line_type_t t);

/* ── Indentation utilities ────────────────────────────────────────────────── */

/*
 * yg_indent_count — count leading space characters.
 *
 * Scans from the start of `line` and counts contiguous space (' ') bytes.
 * Returns -1 if a tab character ('\t') is encountered before any
 * non-whitespace content, signalling a YAML indentation error.
 *
 * Suitable for use by the parser layer as well as the lexer.
 */
int yg_indent_count(const char *line);

/*
 * yg_indent_has_tab — check whether indentation contains a tab.
 *
 * Returns 1 if the first non-space character in the leading whitespace
 * region is a tab, 0 otherwise. Equivalent to (yg_indent_count(line) < 0)
 * but explicitly named for readability at call sites.
 */
int yg_indent_has_tab(const char *line);

#endif /* YAMLGET_LEXER_H */
