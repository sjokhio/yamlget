# Roadmap — v0.1.0

This document defines the complete, frozen scope for the v0.1.0 release of `yamlget`. Nothing outside this scope will be added to v0.1.0. If a contribution addresses something not listed here, it belongs to a later milestone.

## Goal

Ship a working, installable binary that reliably extracts scalar values from YAML mapping keys in common CI/CD configurations.

## Interface

```
yamlget <file> <path>
```

- `<file>` — path to a YAML file, or `-` to read from stdin.
- `<path>` — dot-notation key path. Example: `server.port`, `app.name`, `deploy.image.tag`.

Output goes to **stdout** (the value only, followed by a newline). Diagnostic messages go to **stderr**.

## Exit codes

| Code | Constant | Condition |
|------|----------|-----------|
| 0 | `YAMLGET_EXIT_OK` | Success — value printed |
| 1 | `YAMLGET_EXIT_NOT_FOUND` | Key path not found in document |
| 2 | `YAMLGET_EXIT_BAD_ARGS` | Wrong number of arguments or unrecognised flag |
| 3 | `YAMLGET_EXIT_FILE_ERROR` | File not found, not readable, or stdin read error |
| 4 | `YAMLGET_EXIT_PARSE_ERROR` | Input is not valid YAML (within supported subset) |
| 5 | `YAMLGET_EXIT_INTERNAL` | Unexpected internal state — always a bug |

## YAML subset supported

v0.1.0 supports only what is needed to cover common CI/CD configuration files:

### Supported

- Block mappings (nested, arbitrary depth)
- Flow mappings `{ key: value }` — best-effort; not a primary target
- Scalar types: plain, single-quoted, double-quoted
- Multiline scalars: literal block (`|`) and folded block (`>`)
- Standard boolean literals: `true`, `false`, `yes`, `no`, `on`, `off`
- Null: `~`, `null`, empty value
- Integers and floats as plain scalars (returned as-is, no type conversion)
- Comments (`#`) — ignored
- UTF-8 input

### Explicitly not supported in v0.1.0

- Sequences / arrays (`- item`)
- Multi-document streams (`---` / `...` separators used to end a document)
- Anchors and aliases (`&anchor`, `*alias`)
- Tags (`!!str`, `!!int`, etc.)
- Merge keys (`<<: *defaults`)
- YAML 1.1 vs 1.2 distinction

Encountering any unsupported construct in the **path to the requested key** results in exit code `4`. Encountering it outside the lookup path may be silently ignored, depending on parse strategy.

## Milestones

### M1 — Repository skeleton ✅
- [x] Directory structure
- [x] `Makefile` with `all`, `clean`, `test`, `install` targets
- [x] GitHub Actions CI (Linux gcc, Linux clang, macOS clang, Windows MSVC, Windows MinGW)
- [x] `README.md`, `CONTRIBUTING.md`, `CHANGELOG.md`
- [x] Coding standards document
- [x] Test fixture directory with sample YAML files
- [x] `include/yamlget.h` — public header with exit code constants
- [x] `src/main.c` — argument parsing skeleton with correct exit codes

### M2 — Lexer ✅
- [x] `src/lexer.c` / `include/yamlget/lexer.h`
- [x] Line types: `BLANK`, `COMMENT`, `KEY_ONLY`, `KEY_VALUE`, `INVALID`, `EOF`
- [x] Handles plain, single-quoted (`'...'`), and double-quoted (`"..."`) scalars
- [x] Single-quote escape (`''` → `'`) and common double-quote escapes (`\"`, `\\`, `\n`, `\t`, `\r`)
- [x] Tracks indentation depth (leading space count) per line
- [x] Detects tab-in-indentation and reports as `INVALID` with source location
- [x] Strips inline comments from plain scalar values
- [x] Empty key detection; key and value length bounds checking
- [x] `yg_indent_count()` / `yg_indent_has_tab()` utilities available to parser layer
- [x] 9 fixture-based lexer unit tests; zero ASan/UBSan errors
- [x] `make test` / `make test-lexer` / `make asan-test` targets
- [ ] Block scalars (`|` and `>`) — deferred to M3 or later
- [ ] Column tracking — deferred (line numbers are sufficient for v0.1.0 error messages)

### M3 — Streaming Path Resolution ✅
- [x] `src/parser.c` / `include/yamlget/parser.h`
- [x] `yg_path_split()` — splits dot-notation path into up to 32 segments; rejects empty/trailing/double dots
- [x] `yg_stream_lookup()` — single-pass, O(n) streaming lookup; no AST, no dynamic allocation
- [x] Pop-based indentation stack (max 64 frames) tracks matched vs unmatched nesting levels
- [x] Sibling-branch isolation: unmatched frames block deeper keys from false-positive matching
- [x] Exact key matching — `app.name` cannot match `app.name_extra` or `app.names`
- [x] KEY_ONLY at the target depth prints empty value and exits 0 (key exists, value is null/empty)
- [x] INVALID lexer lines abort with `YAMLGET_EXIT_PARSE_ERROR` (exit 4)
- [x] Stdin support via `-` filename
- [x] `main.c` wired end-to-end: argument parsing → path split → file/stdin open → stream lookup
- [x] 70 integration tests pass (9 lexer + 61 end-to-end); zero ASan/UBSan errors
- [x] `make test` runs both lexer and integration test suites

### M4 — YAML Compatibility Hardening ✅
- [x] Block scalars: literal (`|`) and folded (`>`) fully implemented in the lexer
- [x] All chomping indicators: clip (default), strip (`|-`), keep (`|+`)
- [x] Explicit indentation indicator support (`|2`, `>4`, etc.)
- [x] Folded more-indented line semantics (newline separator instead of space)
- [x] `buf_pending` lookahead slot added to `yg_lexer_t` for block scalar body reading
- [x] CRLF (`\r\n`) line endings validated across block scalar bodies
- [x] Integration test suite expanded from 61 → 102 tests (112 total with lexer)
- [x] New fixtures: `block-scalars.yaml`, `crlf.yaml`, `large.yaml` (1001 keys),
      `malformed-block.yaml`
- [x] `edge-cases.yaml` fully exercised (was untested despite existing in the tree)
- [x] Benchmark suite: `bench/bench.sh` + `bench/fixture.yaml` + `make bench`
- [x] Duplicate-key behavior documented
- [x] `test_lexer.c` updated to escape embedded `\n` in pipe-separated output

### M5 — Integration and polish ✅
- [x] End-to-end integration tests (112 tests, all passing)
- [x] `--version` flag
- [x] `--help` flag — revised with examples, YAML subset summary, exit code table
- [x] GitHub Actions release workflow (`.github/workflows/release.yml`)
  — Linux x86_64, macOS x86_64, macOS arm64, Windows x86_64 binaries
- [x] `docs/release-checklist.md` — step-by-step release procedure
- [x] README completely revised with elevator pitch, download table,
  block scalar examples, benchmarks section, and contributing constraints
- [x] `CHANGELOG.md` updated with `[0.1.0]` release entry and comparison links
- [x] Benchmark results documented (macOS Apple Silicon, 500 iterations):
  yamlget ~1.5 ms/op vs python+PyYAML ~24 ms/op
- [ ] Man page (`docs/yamlget.1`) — deferred to v0.2.0 or v1.0.0
- [ ] `make install` field-tested on target platforms (verify after binary release)
- [ ] All CI jobs green on GitHub (requires push to remote)
- [ ] Tag `v0.1.0` and push

## Non-goals (v0.1.0)

These are explicitly excluded and will not be considered for v0.1.0 even if a patch is submitted:

- Array / sequence indexing
- JSON output
- Shell variable export (`export KEY=value`)
- Shell completion (bash, zsh, fish)
- Multiple file inputs
- Glob patterns in key paths
- YAML writing or editing
- YAML merging
- Multi-document YAML streams

## Success criteria

v0.1.0 ships when:

1. All five CI matrix jobs pass on every commit to `main`.
2. The binary correctly extracts values from all files in `tests/fixtures/`.
3. All exit codes behave as specified in the table above.
4. Binary size is under 200 KB (stripped) on Linux x86-64.
5. Zero memory errors under ASan on all test inputs.
6. `make install` and `make uninstall` work cleanly.
