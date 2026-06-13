# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Initial repository structure and build system
- `README.md`, `CONTRIBUTING.md`, coding standards, and roadmap documents
- GitHub Actions CI workflow for Linux (gcc, clang), macOS, and Windows
- Test fixture directory with sample YAML files
- `Makefile` with `all`, `clean`, `test`, and `install` targets
- MIT license

### Planned for v0.1.0
- Pure C99 YAML parser (scalar and mapping nodes only)
- `yamlget <file> <dot.path>` CLI interface
- stdin support via `-` filename
- Nested mapping lookup by dot-notation path
- Raw value output to stdout, errors to stderr
- Defined exit codes: 0 success, 1 not found, 2 invalid args, 3 file error, 4 parse error, 5 internal

---

[Unreleased]: https://github.com/your-org/yamlget/compare/HEAD...HEAD
