<div align="center">

# yamlget

**Dependency-free YAML key extractor - pure C, zero allocation, instant startup.**

[![CI](https://github.com/sjokhio/yamlget/actions/workflows/ci.yml/badge.svg)](https://github.com/sjokhio/yamlget/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/sjokhio/yamlget?color=blue)](https://github.com/sjokhio/yamlget/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Language: C99](https://img.shields.io/badge/language-C99-blue.svg)](#build-from-source)
[![Platforms](https://img.shields.io/badge/platform-linux%20%7C%20macOS%20%7C%20windows-lightgrey.svg)](#installation)

</div>

---

`yamlget` reads a YAML file and prints the value at a dot-notation path to stdout.
Nothing more. Designed for CI/CD pipelines, shell scripts, and containers where pulling in Python, Node, or `yq` is too heavy.

```sh
$ yamlget config.yaml database.host
localhost

$ yamlget Chart.yaml appVersion
1.4.2

$ VERSION=$(yamlget config.yaml app.version)
```

---

## Why yamlget?

| Tool | Dependency | Typical startup | Purpose |
|------|:----------:|:---------------:|---------|
| `yq` | Go binary | ~5-20 ms | General YAML processor |
| `python -c` | Python interpreter | ~20-50 ms | General scripting |
| `node -e` | Node.js runtime | ~50-100 ms | General scripting |
| **yamlget** | **None** | **~1-2 ms** | **Key extraction only** |

`yamlget` does **one thing**: extract a value by path. It does not edit, merge, transform, or validate YAML. That constraint is a feature: it keeps the binary tiny and the behaviour predictable.

---

## Installation

### Pre-built binaries

Download from the [latest release](https://github.com/sjokhio/yamlget/releases/latest):

| Platform | File |
|----------|------|
| Linux x86_64 | `yamlget-linux-x86_64` |
| macOS x86_64 (Intel) | `yamlget-macos-x86_64` |
| macOS arm64 (Apple Silicon) | `yamlget-macos-arm64` |
| Windows x86_64 | `yamlget-windows-x86_64.exe` |

```sh
# Linux / macOS
curl -Lo yamlget \
  https://github.com/sjokhio/yamlget/releases/latest/download/yamlget-linux-x86_64
chmod +x yamlget
sudo mv yamlget /usr/local/bin/
```

### Build from source

Requirements: any C99 compiler (`gcc`, `clang`, or MSVC). No other dependencies.

```sh
git clone https://github.com/sjokhio/yamlget.git
cd yamlget
make
sudo make install        # installs to /usr/local/bin by default
```

Override the install prefix:

```sh
make install PREFIX=/opt/local
```

#### Windows (MSVC Developer Command Prompt)

```bat
cl /W4 /WX /O2 /std:c17 /D_CRT_SECURE_NO_WARNINGS /Fe:yamlget.exe /Iinclude ^
   src\main.c src\lexer.c src\parser.c
```

### Verify

```sh
yamlget --version   # prints: 0.1.0
yamlget --help
```

---

## Usage

```
yamlget <file> <path>
yamlget - <path>         # read from stdin
```

| Argument | Description |
|----------|-------------|
| `<file>` | Path to a YAML file, or `-` to read from stdin. |
| `<path>` | Dot-notation key path, e.g. `server.port` or `database.host`. |

### Examples

Given `config.yaml`:

```yaml
app:
  name: myservice
  version: 1.4.2

database:
  host: localhost
  port: 5432

deploy:
  region: us-east-1
  replicas: 3
  image:
    tag: latest
```

```sh
yamlget config.yaml app.name          # myservice
yamlget config.yaml database.port     # 5432
yamlget config.yaml deploy.image.tag  # latest
```

### Stdin

```sh
cat config.yaml | yamlget - app.name
kubectl get cm my-config -o yaml | yamlget - data.app_port
```

### Block scalars

`yamlget` handles both literal (`|`) and folded (`>`) block scalars, including all chomping indicators:

```yaml
script: |
  echo "Building..."
  go build ./...

description: >
  A long description that is wrapped
  across multiple lines for readability.

stripped: |-
  no trailing newline

kept: |+
  trailing newlines preserved
```

```sh
$ yamlget config.yaml script
echo "Building..."
go build ./...

$ yamlget config.yaml description
A long description that is wrapped across multiple lines for readability.
```

### Use in CI/CD

```sh
# Extract a version tag
VERSION=$(yamlget Chart.yaml appVersion)
docker build -t "myservice:${VERSION}" .

# Read config at deploy time
REPLICAS=$(yamlget config.yaml deploy.replicas)
kubectl scale deployment myservice --replicas="${REPLICAS}"
```

```yaml
# GitHub Actions step
- name: Read app version
  run: echo "VERSION=$(yamlget config.yaml app.version)" >> $GITHUB_ENV
```

### Exit codes

| Code | Meaning |
|:----:|---------|
| `0` | Key found, value printed to stdout |
| `1` | Key not found |
| `2` | Invalid arguments (wrong count, malformed path) |
| `3` | File error (not found, not readable) |
| `4` | Parse error (invalid or unsupported YAML) |
| `5` | Internal error (always a bug - please report) |

```sh
# Check before use
if yamlget config.yaml feature.enabled >/dev/null 2>&1; then
    ENABLED=$(yamlget config.yaml feature.enabled)
fi
```

---

## Supported YAML subset

`yamlget` targets the mapping-heavy YAML common in CI/CD configuration files.

### Supported

- Block mappings, arbitrarily nested, any indentation width (spaces only)
- Scalar types:
  - Plain: `key: value`
  - Single-quoted: `key: 'hello world'`
  - Double-quoted with escape sequences: `key: "line1\nline2"`
  - Literal block: `key: |` (newlines preserved)
  - Folded block: `key: >` (newlines become spaces)
- All block scalar chomping indicators: `|` (clip), `|-` (strip), `|+` (keep)
- Inline comment stripping (`key: value  # this is ignored`)
- Empty values (`key:` or `key: ""`): prints an empty line, exits 0
- Stdin via `-` filename
- LF (`\n`) and CRLF (`\r\n`) line endings

### Not supported (exits 4)

- Sequences / arrays (`- item`)
- Multi-document streams (`---`)
- Tab-indented files
- Anchors and aliases (`&anchor`, `*alias`)

### Duplicate keys

First occurrence wins. The streaming parser stops at the first full path match.

---

## Benchmarks

Measured on macOS (Apple Silicon) extracting a 3-level nested key from an 80-key CI/CD config.
Timings include full process startup + file parse + output (what a shell script pays per call).

| Tool | ms / invocation | vs yamlget |
|------|:---------------:|:----------:|
| **yamlget 0.1.0** | **~1.5 ms** | 1x (baseline) |
| python3 + PyYAML | ~24 ms | ~16x slower |
| python3 + ruamel.yaml | ~30 ms (est.) | ~20x slower |
| yq (Go) | ~5-15 ms (est.) | ~3-10x slower |

> Numbers are indicative. Process startup dominates for small files. Run `make bench` to measure on your own hardware.

```sh
make bench              # 1000 iterations (default)
make bench BENCH_N=200  # custom iteration count
```

---

## Status

`v0.1.0` - first stable release.

| Milestone | Status |
|-----------|:------:|
| M1: Repository skeleton | done |
| M2: Streaming lexer | done |
| M3: Streaming path resolution | done |
| M4: YAML compatibility hardening | done |
| M5: Polish & release | done |

**Supported in v0.1.0:**

- [x] Streaming lexer: blank / comment / key-only / key-value line classification
- [x] Plain, single-quoted, and double-quoted scalar extraction
- [x] Literal block scalars (`|`) and folded block scalars (`>`)
- [x] All chomping indicators (`-`, `+`)
- [x] Indentation depth tracking via pop-based stack (no AST, no dynamic allocation)
- [x] Dot-notation path lookup with sibling-branch isolation
- [x] Exact key matching (no prefix or suffix bleed)
- [x] Raw value output to stdout; all diagnostics to stderr (filename + line number)
- [x] stdin support via `-`
- [x] LF and CRLF line ending support
- [x] 112 tests (10 lexer unit + 102 integration), zero ASan/UBSan errors
- [x] Pre-built binaries for Linux x86_64, macOS x86_64, macOS arm64, Windows x86_64

**Planned for future releases:**

- [ ] Array / sequence indexing (v0.2.0)
- [ ] JSON output via `--json` (v0.3.0)
- [ ] Shell export format via `--export` (v0.3.0)

See [docs/roadmap-v0.1.0.md](docs/roadmap-v0.1.0.md) and [docs/roadmap-future.md](docs/roadmap-future.md).

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). Contributions are welcome.

**Key constraint:** `yamlget` must remain a dependency-free pure-C binary. No new library dependencies, no new runtime requirements. Open an issue before writing code if you are unsure whether a feature fits.

---

## License

[MIT](LICENSE)
