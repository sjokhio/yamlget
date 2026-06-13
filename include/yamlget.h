#ifndef YAMLGET_H
#define YAMLGET_H

/*
 * yamlget — dependency-free YAML key extractor
 *
 * Public header. Include this in any translation unit that needs exit code
 * constants or the public lookup API (once implemented).
 */

/* ── Version ─────────────────────────────────────────────────────────────── */

#define YAMLGET_VERSION_MAJOR 0
#define YAMLGET_VERSION_MINOR 1
#define YAMLGET_VERSION_PATCH 0
#define YAMLGET_VERSION_STRING "0.1.0"

/* ── Exit codes ──────────────────────────────────────────────────────────── */

/*
 * All exit() calls in the codebase must use one of these constants.
 * Never pass a bare integer literal to exit().
 */
#define YAMLGET_EXIT_OK          0  /* success: value printed to stdout       */
#define YAMLGET_EXIT_NOT_FOUND   1  /* key path not found in document         */
#define YAMLGET_EXIT_BAD_ARGS    2  /* wrong number of args / unknown flag    */
#define YAMLGET_EXIT_FILE_ERROR  3  /* file not found, not readable, etc.     */
#define YAMLGET_EXIT_PARSE_ERROR 4  /* input is not valid (supported) YAML    */
#define YAMLGET_EXIT_INTERNAL    5  /* unexpected internal state — always bug */

#endif /* YAMLGET_H */
