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

/* ── Public API ───────────────────────────────────────────────────────────── */

void yg_lexer_init(yg_lexer_t *lex, FILE *stream, const char *source)
{
    lex->stream  = stream;
    lex->source  = source ? source : "<unknown>";
    lex->lineno  = 0;
    lex->buf[0]  = '\0';
}

int yg_lexer_next(yg_lexer_t *lex, yg_line_t *out)
{
    memset(out, 0, sizeof(*out));

    if (!fgets(lex->buf, sizeof(lex->buf), lex->stream)) {
        if (feof(lex->stream)) {
            out->type   = YG_LINE_EOF;
            out->lineno = lex->lineno + 1;
            return 0;
        }
        fprintf(stderr, "%s: I/O error on line %d\n", lex->source, lex->lineno);
        return YAMLGET_EXIT_FILE_ERROR;
    }

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

    /* ── KEY_VALUE: parse the scalar ────────────────────────────────────── */
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
