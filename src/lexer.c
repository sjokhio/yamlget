/*
 * lexer.c — line-oriented YAML classifier
 *
 * See include/yamlget/lexer.h for the public API and design notes.
 */

#include "yamlget/lexer.h"
#include "yamlget.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* ── Internal helpers ─────────────────────────────────────────────────────── */

static int is_blank_line(const char *s)
{
    while (*s) {
        if (!isspace((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

static void strip_trailing_ws(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
        s[--n] = '\0';
}

/*
 * Strip an inline comment from a plain scalar value in-place.
 * A '#' is a comment delimiter only when preceded by at least one space.
 * This matches YAML spec §6.9 (separation spaces before comments).
 */
static void strip_inline_comment(char *s)
{
    char *p = s;
    while (*p) {
        if (*p == '#' && p > s && isspace((unsigned char)*(p - 1))) {
            *p = '\0';
            strip_trailing_ws(s);
            return;
        }
        p++;
    }
}

/*
 * Copy and unescape a single-quoted YAML scalar into dst[0..dst_max-1].
 *
 * `src` must point to the character immediately after the opening "'".
 * Single-quote escaping rule: '' → ' (the only escape in single-quoted YAML).
 *
 * Returns the number of bytes written (excluding '\0'), or -1 if the closing
 * quote is missing (unterminated string).
 */
static int unescape_single(const char *src, char *dst, int dst_max)
{
    int n = 0;
    while (*src && n < dst_max - 1) {
        if (*src == '\'') {
            if (*(src + 1) == '\'') {   /* '' → ' */
                dst[n++] = '\'';
                src += 2;
            } else {
                break;                  /* closing quote */
            }
        } else {
            dst[n++] = *src++;
        }
    }
    dst[n] = '\0';
    /* success if we stopped at closing quote or at string end */
    return (*src == '\'' || *src == '\0') ? n : -1;
}

/*
 * Copy and unescape a double-quoted YAML scalar into dst[0..dst_max-1].
 *
 * `src` must point to the character immediately after the opening '"'.
 * Recognised escape sequences: \", \\, \n, \t, \r, \0.
 * Unrecognised sequences are passed through as two characters.
 *
 * Returns the number of bytes written (excluding '\0'), or -1 if the closing
 * quote is missing.
 */
static int unescape_double(const char *src, char *dst, int dst_max)
{
    int n = 0;
    while (*src && *src != '"' && n < dst_max - 1) {
        if (*src == '\\') {
            src++;
            switch (*src) {
                case '"':  dst[n++] = '"';  src++; break;
                case '\\': dst[n++] = '\\'; src++; break;
                case 'n':  dst[n++] = '\n'; src++; break;
                case 't':  dst[n++] = '\t'; src++; break;
                case 'r':  dst[n++] = '\r'; src++; break;
                case '0':  dst[n++] = '\0'; src++; break;
                case '\0': dst[n++] = '\\';        break; /* trailing backslash */
                default:
                    /* Pass through unrecognised sequences verbatim. */
                    if (n < dst_max - 2) { dst[n++] = '\\'; }
                    dst[n++] = *src++;
                    break;
            }
        } else {
            dst[n++] = *src++;
        }
    }
    dst[n] = '\0';
    return (*src == '"' || *src == '\0') ? n : -1;
}

/*
 * Locate the ':' that acts as the key/value separator in a block mapping.
 *
 * Per YAML spec §7.4, the indicator ':' is a mapping separator only when
 * followed by safe whitespace (space, tab) or end of content. A ':' inside
 * a quoted string is skipped transparently.
 *
 * Returns a pointer to the ':' character, or NULL if none is found.
 */
static const char *find_mapping_colon(const char *s)
{
    char in_quote = 0; /* '\'' or '"', 0 when not inside a quoted string */

    while (*s) {
        if (in_quote) {
            if (*s == '\\' && in_quote == '"') {
                s++; /* skip escaped character inside double-quoted string */
                if (*s) s++;
                continue;
            }
            if (*s == in_quote)
                in_quote = 0;
        } else {
            if (*s == '\'' || *s == '"') {
                in_quote = *s;
            } else if (*s == ':') {
                char next = *(s + 1);
                if (next == ' ' || next == '\t' || next == '\0')
                    return s;
            }
        }
        s++;
    }
    return NULL;
}

/* ── Indentation utilities ────────────────────────────────────────────────── */

int yg_indent_count(const char *line)
{
    int n = 0;
    while (*line) {
        if (*line == ' ') {
            n++;
            line++;
        } else if (*line == '\t') {
            return -1; /* tab in indentation position */
        } else {
            break;
        }
    }
    return n;
}

int yg_indent_has_tab(const char *line)
{
    while (*line) {
        if (*line == '\t') return 1;
        if (*line != ' ')  return 0;
        line++;
    }
    return 0;
}

/* ── Block scalar reader ──────────────────────────────────────────────────── */

/*
 * Read and assemble a block scalar value (| literal or > folded).
 *
 * Called after the `key: |` or `key: >` line has been processed by
 * yg_lexer_next(). Reads subsequent lines from lex->stream, building the
 * assembled value into out->value.
 *
 * Parameters:
 *   style          — '|' (literal) or '>' (folded)
 *   chomping       — 0 (clip, default), '-' (strip), '+' (keep)
 *   explicit_indent — 0 = auto-detect, N = body indent is key_indent + N
 *   key_indent     — leading spaces on the `key: |` line
 *
 * When a line is encountered that is less indented than the block body, it is
 * "put back" by setting lex->buf_pending = 1 and decrementing lex->lineno so
 * the next call to yg_lexer_next() processes it correctly.
 *
 * Returns 0 on success (out->type is set), or YAMLGET_EXIT_FILE_ERROR on I/O
 * error. Errors set out->type = YG_LINE_INVALID before returning 0 so the
 * caller can treat them uniformly.
 */
static int read_block_scalar(yg_lexer_t *lex, yg_line_t *out,
                             char style, char chomping, int explicit_indent,
                             int key_indent)
{
    int body_indent   = explicit_indent > 0 ? key_indent + explicit_indent : 0;
    int value_len     = 0;
    int trailing_nl   = 0;  /* blank lines since last content */
    int first_content = 1;  /* no content line seen yet */
    int prev_more     = 0;  /* for folded: prev content was more-indented */

    out->value[0] = '\0';

    for (;;) {
        /* Fetch next line: use lookahead if available, else read from stream. */
        if (!lex->buf_pending) {
            if (!fgets(lex->buf, sizeof(lex->buf), lex->stream)) {
                if (!feof(lex->stream)) {
                    fprintf(stderr, "%s: I/O error on line %d\n",
                            lex->source, lex->lineno);
                    out->type = YG_LINE_INVALID;
                    return YAMLGET_EXIT_FILE_ERROR;
                }
                break; /* EOF ends the block */
            }
            lex->lineno++;
        }
        lex->buf_pending = 0;

        /* Normalise line endings. */
        size_t tlen = strlen(lex->buf);
        if (tlen > 0 && lex->buf[tlen - 1] == '\n') lex->buf[--tlen] = '\0';
        if (tlen > 0 && lex->buf[tlen - 1] == '\r') lex->buf[--tlen] = '\0';

        /* Blank line within block. */
        if (is_blank_line(lex->buf)) {
            trailing_nl++;
            continue;
        }

        /* Check indentation. */
        int this_indent = yg_indent_count(lex->buf);
        if (this_indent < 0) {
            fprintf(stderr, "%s:%d: tab in indentation (use spaces)\n",
                    lex->source, lex->lineno);
            out->type = YG_LINE_INVALID;
            return 0;
        }

        /* Determine body indentation from first non-blank content line. */
        if (body_indent == 0) {
            if (this_indent <= key_indent) {
                /* First non-blank line is not more-indented: empty block. */
                lex->buf_pending = 1;
                lex->lineno--;
                break;
            }
            body_indent = this_indent;
        }

        /* Line less indented than the block body ends the block. */
        if (this_indent < body_indent) {
            lex->buf_pending = 1;
            lex->lineno--;
            break;
        }

        int this_more = (this_indent > body_indent);

        /* Emit separator between previously-emitted content and this line. */
        if (trailing_nl > 0) {
            /*
             * Blank lines between content lines are always literal newlines
             * in both literal and folded styles.
             */
            int i;
            for (i = 0; i < trailing_nl && value_len < YG_VALUE_MAX - 1; i++)
                out->value[value_len++] = '\n';
            trailing_nl = 0;
        } else if (!first_content) {
            if (style == '|') {
                /* Literal: every line break becomes '\n'. */
                if (value_len < YG_VALUE_MAX - 1)
                    out->value[value_len++] = '\n';
            } else {
                /* Folded ('>'): adjacent normal lines are folded with a
                 * space. Lines adjacent to a more-indented line use '\n'.
                 * (YAML spec §8.1.1.2)
                 */
                char sep = (prev_more || this_more) ? '\n' : ' ';
                if (value_len < YG_VALUE_MAX - 1)
                    out->value[value_len++] = sep;
            }
        }
        first_content = 0;
        prev_more     = this_more;

        /* Copy content with body indentation stripped. */
        const char *content = lex->buf + body_indent;
        size_t clen = strlen(content);
        while (clen > 0 && (content[clen - 1] == ' ' || content[clen - 1] == '\t'))
            clen--;

        if (value_len + (int)clen >= YG_VALUE_MAX - 1) {
            fprintf(stderr, "%s:%d: block scalar exceeds maximum length (%d bytes)\n",
                    lex->source, lex->lineno, YG_VALUE_MAX - 1);
            out->type = YG_LINE_INVALID;
            return 0;
        }
        memcpy(out->value + value_len, content, clen);
        value_len += (int)clen;
    }

    /*
     * Apply chomping for trailing blank lines.
     *
     * clip (0): discard trailing blanks. The printf("%s\n") in the caller
     *           adds the single final newline mandated by clip semantics.
     * strip('-'): discard trailing blanks. Same as clip at this layer.
     * keep ('+'): add the trailing blank lines as '\n' chars in the value.
     *             One of those newlines is then removed so printf's addition
     *             does not produce a double-newline for the clip/keep boundary.
     */
    if (chomping == '+' && trailing_nl > 0) {
        int i;
        for (i = 0; i < trailing_nl && value_len < YG_VALUE_MAX - 1; i++)
            out->value[value_len++] = '\n';
        /* Remove the last '\n' — printf("%s\n") will re-add it. */
        if (value_len > 0 && out->value[value_len - 1] == '\n')
            value_len--;
    }

    out->value[value_len] = '\0';
    out->type      = YG_LINE_KEY_VALUE;
    out->has_value = 1;
    return 0;
}

/* ── Public API ───────────────────────────────────────────────────────────── */

void yg_lexer_init(yg_lexer_t *lex, FILE *stream, const char *source)
{
    lex->stream      = stream;
    lex->source      = source ? source : "<unknown>";
    lex->lineno      = 0;
    lex->buf[0]      = '\0';
    lex->buf_pending = 0;
}

int yg_lexer_next(yg_lexer_t *lex, yg_line_t *out)
{
    memset(out, 0, sizeof(*out));

    /*
     * If a lookahead line was saved by read_block_scalar (because it belonged
     * to the outer context), use it directly without reading from the stream.
     * The lineno was decremented when the line was put back, so incrementing
     * here restores the correct line number for this record.
     */
    if (!lex->buf_pending) {
        if (!fgets(lex->buf, sizeof(lex->buf), lex->stream)) {
            if (feof(lex->stream)) {
                out->type   = YG_LINE_EOF;
                out->lineno = lex->lineno + 1;
                return 0;
            }
            fprintf(stderr, "%s: I/O error on line %d\n", lex->source, lex->lineno);
            return YAMLGET_EXIT_FILE_ERROR;
        }
    }
    lex->buf_pending = 0;
    lex->lineno++;
    out->lineno = lex->lineno;

    /* Normalise line endings. */
    size_t len = strlen(lex->buf);
    if (len > 0 && lex->buf[len - 1] == '\n') lex->buf[--len] = '\0';
    if (len > 0 && lex->buf[len - 1] == '\r') lex->buf[--len] = '\0';

    /* ── Blank line ─────────────────────────────────────────────────────── */
    if (is_blank_line(lex->buf)) {
        out->type = YG_LINE_BLANK;
        return 0;
    }

    /* ── Indentation ────────────────────────────────────────────────────── */
    int indent = yg_indent_count(lex->buf);
    if (indent < 0) {
        fprintf(stderr, "%s:%d: tab in indentation (use spaces)\n",
                lex->source, lex->lineno);
        out->type   = YG_LINE_INVALID;
        out->indent = 0;
        return 0;
    }
    out->indent = indent;

    const char *p = lex->buf + indent; /* first non-space character */

    /* ── Comment line ───────────────────────────────────────────────────── */
    if (*p == '#') {
        out->type = YG_LINE_COMMENT;
        return 0;
    }

    /* ── Mapping entry ──────────────────────────────────────────────────── */
    const char *colon = find_mapping_colon(p);
    if (!colon) {
        fprintf(stderr, "%s:%d: expected mapping entry (key: value)\n",
                lex->source, lex->lineno);
        out->type = YG_LINE_INVALID;
        return 0;
    }

    /* Extract key — everything before ':', stripped of trailing whitespace. */
    size_t key_len = (size_t)(colon - p);
    while (key_len > 0 && isspace((unsigned char)p[key_len - 1]))
        key_len--;

    if (key_len == 0) {
        fprintf(stderr, "%s:%d: mapping entry has empty key\n",
                lex->source, lex->lineno);
        out->type = YG_LINE_INVALID;
        return 0;
    }
    if (key_len >= YG_KEY_MAX) {
        fprintf(stderr, "%s:%d: key exceeds maximum length (%d bytes)\n",
                lex->source, lex->lineno, YG_KEY_MAX - 1);
        out->type = YG_LINE_INVALID;
        return 0;
    }
    memcpy(out->key, p, key_len);
    out->key[key_len] = '\0';

    /* Advance past ':' and optional whitespace. */
    const char *after = colon + 1;
    while (*after == ' ' || *after == '\t')
        after++;

    /* ── KEY_ONLY: nothing (or only a comment) after the colon ─────────── */
    if (*after == '\0' || *after == '#') {
        out->type      = YG_LINE_KEY_ONLY;
        out->has_value = 0;
        return 0;
    }

    /* ── Block scalar: | (literal) or > (folded) ────────────────────────── */
    if (*after == '|' || *after == '>') {
        char style    = *after;
        char chomping = 0;         /* 0 = clip (YAML default) */
        int  xindent  = 0;        /* 0 = auto-detect indentation */

        /*
         * Parse the optional block scalar header characters:
         * [ chomping-indicator ] [ indentation-indicator ]
         * in either order, as per YAML spec §8.1.1.
         */
        const char *h = after + 1;
        while (*h && *h != ' ' && *h != '\t' && *h != '#') {
            if (*h == '+' || *h == '-') {
                chomping = *h;
            } else if (*h >= '1' && *h <= '9') {
                xindent = *h - '0';
            }
            h++;
        }

        return read_block_scalar(lex, out, style, chomping, xindent, out->indent);
    }

    /* ── KEY_VALUE: parse the inline scalar ─────────────────────────────── */
    out->type      = YG_LINE_KEY_VALUE;
    out->has_value = 1;

    if (*after == '\'') {
        if (unescape_single(after + 1, out->value, YG_VALUE_MAX) < 0) {
            fprintf(stderr, "%s:%d: unterminated single-quoted scalar\n",
                    lex->source, lex->lineno);
            out->type = YG_LINE_INVALID;
        }
    } else if (*after == '"') {
        if (unescape_double(after + 1, out->value, YG_VALUE_MAX) < 0) {
            fprintf(stderr, "%s:%d: unterminated double-quoted scalar\n",
                    lex->source, lex->lineno);
            out->type = YG_LINE_INVALID;
        }
    } else {
        /* Plain scalar: guard length, copy, then strip trailing noise. */
        size_t val_len = strlen(after);
        if (val_len >= YG_VALUE_MAX) {
            fprintf(stderr, "%s:%d: scalar value exceeds maximum length (%d bytes)\n",
                    lex->source, lex->lineno, YG_VALUE_MAX - 1);
            out->type = YG_LINE_INVALID;
            return 0;
        }
        memcpy(out->value, after, val_len + 1);
        strip_inline_comment(out->value);
        strip_trailing_ws(out->value);
    }

    return 0;
}

const char *yg_line_type_name(yg_line_type_t t)
{
    switch (t) {
        case YG_LINE_BLANK:     return "BLANK";
        case YG_LINE_COMMENT:   return "COMMENT";
        case YG_LINE_KEY_ONLY:  return "KEY_ONLY";
        case YG_LINE_KEY_VALUE: return "KEY_VALUE";
        case YG_LINE_INVALID:   return "INVALID";
        case YG_LINE_EOF:       return "EOF";
    }
    return "UNKNOWN";
}
