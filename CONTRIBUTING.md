# Contributing to yamlget

Thank you for your interest in contributing. `yamlget` is intentionally a small, focused tool. Please read this document before opening an issue or pull request.

## Project scope

`yamlget` extracts values from YAML files by dot-notation path. It is **not** a general-purpose YAML processor.

Before contributing a new feature, ask: *does this belong in a key extractor?* Features that are out of scope will not be accepted regardless of implementation quality. When in doubt, open an issue first to discuss.

**In scope for v0.1.0 (shipped):**
- Dot-notation path lookup in nested mappings
- Scalar value output (strings, integers, booleans, floats, null)
- Block scalars: literal (`|`) and folded (`>`) with all chomping indicators
- stdin support
- LF and CRLF line ending support
- Predictable exit codes
- Cross-platform builds (Linux, macOS, Windows)

**Out of scope (now and long-term):**
- YAML editing, rewriting, or merging
- Schema validation
- Multi-document stream processing
- Templating or expression evaluation

**Planned for future releases:**
- Array indexing
- JSON output format
- Shell export format
- Shell completion scripts
- Multi-file matching

## Getting started

1. Fork the repository and create a branch from `main`.
2. Build the project locally:

   ```sh
   make
   ```

3. Run the tests:

   ```sh
   make test
   ```

4. Run the sanitizer suite:

   ```sh
   make asan-test
   ```

5. Make your changes following the [coding standards](docs/coding-standards.md).
6. Add or update test fixtures in `tests/fixtures/` for any behaviour you change.
7. Open a pull request against `main`.

## Pull request guidelines

- **One concern per PR.** Bug fixes and features should be separate PRs.
- **Tests are required.** PRs that change behaviour without updating tests will not be merged.
- **No new dependencies.** `yamlget` must remain dependency-free. Do not add library calls beyond the C standard library and POSIX.
- **Keep the diff small.** Avoid unrelated whitespace changes, reformatting, or refactors in the same PR.
- **Write a clear PR description.** Explain what the change does and why. Reference any related issues.

## Coding standards

See [docs/coding-standards.md](docs/coding-standards.md) for the full style guide.

Summary:
- C99, no compiler extensions
- `snake_case` for all identifiers
- No dynamic memory allocation in the hot path where avoidable
- All error paths must set a specific exit code
- No `printf` in library functions (only in `main.c`)
- All diagnostic output goes to `stderr`; stdout is reserved for extracted values

## Reporting bugs

Open a GitHub issue and include:

1. The command you ran
2. The YAML input (or a minimal reproduction)
3. The actual output and expected output
4. Your OS, compiler, and compiler version

## Commit messages

Use the imperative mood in the subject line:

```
Add dot-notation path parser
Fix off-by-one in scalar termination
Remove unused include in lexer
```

Keep the subject under 72 characters. Add a body paragraph if the motivation is non-obvious.

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](LICENSE).
