# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added — M3 (Streaming Path Resolution)
- `src/parser.c` / `include/yamlget/parser.h` — streaming dot-path resolver
- `yg_path_split()`: splits dot-notation path into segments; rejects leading/trailing/double dots
- `yg_stream_lookup()`: single-pass streaming lookup using a pop-based indentation stack (no AST,
  no dynamic allocation)
- Sibling-branch isolation: unmatched mapping keys push "blocked" frames so deeper lines
  in the wrong branch cannot false-positive match later path segments
- Exact key matching — `app.name` is never matched by `app.name_suffix` or `app.names`
- KEY_ONLY at the target depth prints an empty line and exits 0 (null/empty value is valid)
- INVALID lexer lines propagate immediately as `YAMLGET_EXIT_PARSE_ERROR` (exit 4)
- `main.c` updated: file/stdin opening, path validation, full end-to-end wiring
- Stdin support via `-` filename
- Integration test suite expanded to 61 end-to-end tests (70 total with lexer tests)
- `tests/fixtures/exact-match.yaml` — fixture for exact key-matching verification
- `tests/fixtures/invalid.yaml` revised: uses YAML sequences to trigger parse error predictably
- `tests/run_tests.sh` updated with `check_exact` and `check_stdin` helpers
- CI workflow updated: all five jobs now run `make test` (lexer + integration)
- `make test` now covers both lexer and integration tests; `make asan-test` covers both with
  ASan+UBSan

### Added — M2 (Streaming Lexer Foundation)
- `src/lexer.c` — streaming line-oriented YAML lexer; no dynamic allocation
- `include/yamlget/lexer.h` — public lexer API (`yg_lexer_t`, `yg_line_t`, `yg_line_type_t`)
- Line classification: `BLANK`, `COMMENT`, `KEY_ONLY`, `KEY_VALUE`, `INVALID`, `EOF`
- Indentation tracking: `yg_indent_count()`, `yg_indent_has_tab()` utilities
- Plain scalar extraction with inline-comment stripping
- Single-quoted scalar unescaping (`''` → `'`)
- Double-quoted scalar unescaping (`\"`, `\\`, `\n`, `\t`, `\r`, `\0`)
- Tab-in-indentation detection with file+line error messages to stderr
- Empty key and oversized key/value detection
- `tests/lexer/test_lexer.c` — standalone lexer driver binary
- `tests/lexer/run_lexer_tests.sh` — fixture-based test runner
- 9 lexer fixture pairs: flat mappings, nested mappings, empty values, comments,
  quoted scalars, mixed spacing, invalid tabs, missing colon, empty key
- `make test` now runs lexer unit tests; `make test-all` for future integration tests
- `make asan-test` target for ASan+UBSan clean run
- CI workflow updated: all five matrix jobs now build and test the lexer

### Added — M1 (Repository Skeleton)
- Initial repository structure and build system
- `README.md`, `CONTRIBUTING.md`, coding standards, and roadmap documents
- GitHub Actions CI workflow for Linux (gcc, clang), macOS, and Windows
- Test fixture directory with sample YAML files
- `Makefile` with `all`, `clean`, `test`, and `install` targets
- MIT license

---

[Unreleased]: https://github.com/your-org/yamlget/compare/HEAD...HEAD
