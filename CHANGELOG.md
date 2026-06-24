# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

---

## [0.2.0] - 2026-06-20

### Added: M6 (Block Sequence Lookup)

- Bracket-index path syntax: `key[N]` per dot-notation segment
  (e.g. `servers[0].host`, `pipeline.steps[2].name`, `tags[1]`)
- `yg_path_seg_t` struct: each path segment now carries an optional
  `has_index` / `index` pair alongside the key name
- `yg_path_split()` updated to parse and validate bracket indexes;
  rejects: empty `[]`, negative values, non-decimal content, leading zeros
  on multi-digit numbers, unclosed brackets, content after `]`, overflow
- Lexer: three new line types - `YG_LINE_SEQ_SCALAR`, `YG_LINE_SEQ_MAPPING`,
  `YG_LINE_SEQ_EMPTY` - for block sequence items (`- value`, `- key: val`, `-`)
- Flow sequence/mapping items (`- {k: v}`, `- [a, b]`) and block scalar
  sequence items (`- |`, `- >`) emit `YG_LINE_INVALID` (exit 4; deferred)
- Parser: `frame_t` extended with `kind` (MAPPING/SEQUENCE), `seq_current`,
  `seq_target`; when a matched segment has `has_index`, a `YG_FRAME_SEQUENCE`
  frame is pushed and items are counted until `seq_current == seq_target`
- Sequences outside the active lookup path are now skipped gracefully (exit 1)
  instead of exiting 4; this is a compatibility improvement for files that mix
  mappings and sequences
- `tests/fixtures/sequences.yaml`: comprehensive block sequence fixture
  (flat scalars, mapping items, nested sequences, empty items, quoted items)
- `tests/lexer/fixtures/sequences.yaml` + `.expected`: lexer unit test covering
  all three new sequence line types
- 18 new integration tests; total: 132 integration + 11 lexer = 143 tests
- Path syntax rejection tests (10 cases) verify all malformed bracket forms
  exit 2 without opening the input file

### Changed

- `yg_stream_lookup()` parameter type: `char[][YG_KEY_MAX]` -> `yg_path_seg_t *`
  (internal API; no effect on CLI callers)
- Two v0.1.0 integration tests updated: sequences encountered off the lookup
  path now exit 1 (not found) rather than 4 (parse error)
- `tests/fixtures/invalid.yaml` comment updated to reflect v0.2.0 behavior

---

## [0.1.0] - 2026-06-12

### Added: M5 (Polish & Release)
- `--help` text expanded: examples section, supported/unsupported YAML summary,
  explicit note that stdout is data-only and stderr carries diagnostics
- Pre-built release binaries for Linux x86_64, macOS x86_64, macOS arm64,
  and Windows x86_64 via GitHub Actions release workflow
- `.github/workflows/release.yml`: triggered by `v*.*.*` tags; builds all
  four platform binaries, smoke-tests each, and publishes a GitHub Release
  with auto-generated notes
- `docs/release-checklist.md`: step-by-step checklist for cutting a release
- `bench/bench.sh` and `bench/fixture.yaml` documented in README with
  preliminary benchmark numbers (macOS Apple Silicon, 500 iterations):
  yamlget ~1.5 ms/op vs python+PyYAML ~24 ms/op (~16x faster)
- `make bench` supports `BENCH_N=<n>` override for iteration count
- README completely revised: elevator pitch, download table, block scalar
  usage examples, benchmarks section, CI badge placeholders, duplicate-key
  behaviour documented, contributing constraints documented

### Added: M4 (YAML Compatibility Hardening)
- Block scalar support: literal (`|`) and folded (`>`) styles
  - All chomping indicators: clip (default), strip (`-`), keep (`+`)
  - Optional explicit indentation indicator (`|2`, `>4`, etc.)
  - Folded adjacent-to-more-indented line rule: newline separator instead of space
  - Zero dynamic allocation: body assembled directly into `yg_line_t.value`
  - Streaming: body lines consumed within a single `yg_lexer_next()` call via
    a `buf_pending` lookahead slot in `yg_lexer_t`
- CRLF (`\r\n`) line ending support: existing stripping logic already covered
  LF; extended to validate CRLF across block scalar body lines
- `tests/fixtures/block-scalars.yaml`: 12 block scalar scenarios
- `tests/fixtures/crlf.yaml`: CRLF line endings fixture
- `tests/fixtures/large.yaml`: 1001-key flat fixture for scale validation
- `tests/fixtures/malformed-block.yaml`: tab-in-block-body triggers exit 4
- `tests/fixtures/edge-cases.yaml` now fully exercised (literal block, folded
  block, deep nesting); previously untested despite existing in the tree
- Integration test suite expanded from 61 to 102 end-to-end tests (112 total)
- `tests/lexer/fixtures/block-scalar.yaml` + `.expected`: lexer unit test for
  all four block scalar styles (literal, folded, strip, keep)
- `tests/lexer/test_lexer.c` updated to escape embedded `\n` in value output
  as `\n` (two characters), keeping one record per output line
- `bench/bench.sh`: benchmark comparing yamlget, yq, and python+PyYAML;
  measures full process startup + parse + output per invocation
- `bench/fixture.yaml`: realistic 80-key CI/CD config used as benchmark input
- `make bench` target; supports `BENCH_N=<n>` to set iteration count
- Duplicate-key behavior documented: first occurrence wins (streaming parser
  stops at first full match)

### Added: M3 (Streaming Path Resolution)
- `src/parser.c` / `include/yamlget/parser.h`: streaming dot-path resolver
- `yg_path_split()`: splits dot-notation path into segments; rejects leading/trailing/double dots
- `yg_stream_lookup()`: single-pass streaming lookup using a pop-based indentation stack (no AST,
  no dynamic allocation)
- Sibling-branch isolation: unmatched mapping keys push "blocked" frames so deeper lines
  in the wrong branch cannot false-positive match later path segments
- Exact key matching: `app.name` is never matched by `app.name_suffix` or `app.names`
- KEY_ONLY at the target depth prints an empty line and exits 0 (null/empty value is valid)
- INVALID lexer lines propagate immediately as `YAMLGET_EXIT_PARSE_ERROR` (exit 4)
- `main.c` updated: file/stdin opening, path validation, full end-to-end wiring
- Stdin support via `-` filename
- Integration test suite expanded to 61 end-to-end tests (70 total with lexer tests)
- `tests/fixtures/exact-match.yaml`: fixture for exact key-matching verification
- `tests/fixtures/invalid.yaml` revised: uses YAML sequences to trigger parse error predictably
- `tests/run_tests.sh` updated with `check_exact` and `check_stdin` helpers
- CI workflow updated: all five jobs now run `make test` (lexer + integration)
- `make test` now covers both lexer and integration tests; `make asan-test` covers both with
  ASan+UBSan

### Added: M2 (Streaming Lexer Foundation)
- `src/lexer.c`: streaming line-oriented YAML lexer; no dynamic allocation
- `include/yamlget/lexer.h`: public lexer API (`yg_lexer_t`, `yg_line_t`, `yg_line_type_t`)
- Line classification: `BLANK`, `COMMENT`, `KEY_ONLY`, `KEY_VALUE`, `INVALID`, `EOF`
- Indentation tracking: `yg_indent_count()`, `yg_indent_has_tab()` utilities
- Plain scalar extraction with inline-comment stripping
- Single-quoted scalar unescaping (`''` -> `'`)
- Double-quoted scalar unescaping (`\"`, `\\`, `\n`, `\t`, `\r`, `\0`)
- Tab-in-indentation detection with file+line error messages to stderr
- Empty key and oversized key/value detection
- `tests/lexer/test_lexer.c`: standalone lexer driver binary
- `tests/lexer/run_lexer_tests.sh`: fixture-based test runner
- 9 lexer fixture pairs: flat mappings, nested mappings, empty values, comments,
  quoted scalars, mixed spacing, invalid tabs, missing colon, empty key
- `make test` now runs lexer unit tests; `make test-all` for future integration tests
- `make asan-test` target for ASan+UBSan clean run
- CI workflow updated: all five matrix jobs now build and test the lexer

### Added: M1 (Repository Skeleton)
- Initial repository structure and build system
- `README.md`, `CONTRIBUTING.md`, coding standards, and roadmap documents
- GitHub Actions CI workflow for Linux (gcc, clang), macOS, and Windows
- Test fixture directory with sample YAML files
- `Makefile` with `all`, `clean`, `test`, and `install` targets
- MIT license

---

[Unreleased]: https://github.com/sjokhio/yamlget/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/sjokhio/yamlget/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/sjokhio/yamlget/releases/tag/v0.1.0
