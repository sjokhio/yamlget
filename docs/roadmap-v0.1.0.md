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

### M2 — Lexer
- [ ] `src/lexer.c` / `include/yamlget/lexer.h`
- [ ] Token types: `KEY`, `VALUE`, `INDENT`, `DEDENT`, `NEWLINE`, `EOF`, `ERROR`
- [ ] Handles plain, single-quoted, and double-quoted scalars
- [ ] Handles `|` and `>` block scalars
- [ ] Tracks line and column numbers for error messages
- [ ] Tests: `tests/lexer/`

### M3 — Parser
- [ ] `src/parser.c` / `include/yamlget/parser.h`
- [ ] Recursive descent over lexer output
- [ ] Builds an in-memory tree of `yaml_node_t` (mappings and scalars only)
- [ ] Strict memory safety — verified with ASan
- [ ] Tests: `tests/parser/`

### M4 — Lookup
- [ ] `src/lookup.c` / `include/yamlget/lookup.h`
- [ ] Splits dot-notation path into segments
- [ ] Walks the parsed tree
- [ ] Returns the scalar string value or `NOT_FOUND`
- [ ] Tests: `tests/lookup/`

### M5 — Integration and polish
- [ ] End-to-end integration tests against `tests/fixtures/`
- [ ] `--version` flag
- [ ] `--help` flag
- [ ] Man page (`docs/yamlget.1`)
- [ ] `make install` tested on Linux and macOS
- [ ] All CI jobs green
- [ ] `CHANGELOG.md` updated
- [ ] Tag `v0.1.0`

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
