# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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
