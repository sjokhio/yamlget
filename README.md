# yamlget

[![CI](https://github.com/your-org/yamlget/actions/workflows/ci.yml/badge.svg)](https://github.com/your-org/yamlget/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/your-org/yamlget)](https://github.com/your-org/yamlget/releases/latest)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A dependency-free YAML key extractor written in pure C.

`yamlget` reads a YAML file and prints the value at a dot-notation path to stdout. Nothing more. It is designed for CI/CD pipelines, shell scripts, containers, and any environment where pulling in Python, Node, or `yq` is too heavy.

```sh
$ yamlget config.yaml database.host
localhost

$ yamlget Chart.yaml appVersion
1.4.2

$ VERSION=$(yamlget config.yaml app.version)
```

## Why yamlget?

| Tool | Dependency | Typical startup | Purpose |
|------|------------|-----------------|---------|
| `yq` | Go runtime / binary | ~5–20 ms | General YAML processor |
| `python -c` | Python interpreter | ~20–50 ms | General scripting |
| `node -e` | Node.js runtime | ~50–100 ms | General scripting |
| **yamlget** | **None** | **~1–2 ms** | **Key extraction only** |

`yamlget` does **one thing**: extract a value by path. It does not edit, merge, transform, or validate YAML. That constraint is a feature — it keeps the binary tiny and the behaviour predictable.

## Status

v0.1.0 — first stable release.

| Milestone | Status |
|-----------|--------|
| M1 — Repository skeleton | ✅ Complete |
| M2 — Streaming lexer | ✅ Complete |
| M3 — Streaming path resolution | ✅ Complete |
| M4 — YAML compatibility hardening | ✅ Complete |
| M5 — Polish & release | ✅ Complete |

## Installation

### Download a pre-built binary

Pre-built binaries are attached to every [GitHub release](https://github.com/your-org/yamlget/releases/latest).

| Platform | File |
|----------|------|
| Linux x86_64 | `yamlget-linux-x86_64` |
| macOS x86_64 (Intel) | `yamlget-macos-x86_64` |
| macOS arm64 (Apple Silicon) | `yamlget-macos-arm64` |
| Windows x86_64 | `yamlget-windows-x86_64.exe` |

```sh
# Linux example
curl -Lo yamlget \
  https://github.com/your-org/yamlget/releases/latest/download/yamlget-linux-x86_64
chmod +x yamlget
sudo mv yamlget /usr/local/bin/
```

### Build from source

Requirements: a C99-compatible compiler (`gcc`, `clang`, or MSVC). No other dependencies.

```sh
git clone https://github.com/your-org/yamlget.git
cd yamlget
make
sudo make install        # installs to /usr/local/bin by default
```

Override the install prefix:

```sh
make install PREFIX=/opt/local
```

#### Windows (MSVC)

```bat
cl /W4 /WX /O2 /std:c17 /D_CRT_SECURE_NO_WARNINGS /Fe:yamlget.exe /Iinclude ^
   src\main.c src\lexer.c src\parser.c
```

### Verify the installation

```sh
yamlget --version   # prints 0.1.0
yamlget --help
```

## Usage

```
yamlget <file> <path>
```

| Argument | Description |
|----------|-------------|
| `<file>` | Path to the YAML file. Use `-` to read from stdin. |
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

### Reading from stdin

```sh
cat config.yaml | yamlget - app.name
kubectl get cm my-config -o yaml | yamlget - data.app_port
```

### Block scalars

`yamlget` handles both literal (`|`) and folded (`>`) block scalars:

```yaml
script: |
  echo "Building..."
  go build ./...

description: >
  A long description that is wrapped
  across multiple lines for readability.
```

```sh
yamlget config.yaml script
# echo "Building..."
# go build ./...

yamlget config.yaml description
# A long description that is wrapped across multiple lines for readability.
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
# GitHub Actions
- name: Read app version
  run: echo "VERSION=$(yamlget config.yaml app.version)" >> $GITHUB_ENV
```

### Exit codes

| Code | Meaning |
|------|---------|
| `0` | Success — value printed to stdout |
| `1` | Key not found |
| `2` | Invalid arguments (wrong count, malformed path) |
| `3` | File error (not found, not readable) |
| `4` | Parse error (invalid or unsupported YAML) |
| `5` | Internal error (always a bug — please report) |

```sh
# Pattern: test before use
if yamlget config.yaml feature.enabled >/dev/null 2>&1; then
    ENABLED=$(yamlget config.yaml feature.enabled)
fi
```

## Supported YAML subset

`yamlget` targets the mapping-heavy YAML common in CI/CD configuration files.

### Supported

- Block mappings, arbitrarily nested, any indentation width (spaces only — no tabs)
- Scalar types:
  - Plain: `key: value`
  - Single-quoted: `key: 'hello world'`
  - Double-quoted with escape sequences: `key: "line1\nline2"`
  - Literal block: `key: |` (newlines preserved)
  - Folded block: `key: >` (newlines become spaces)
- All block scalar chomping indicators: `|` (clip), `|-` (strip), `|+` (keep)
- Inline comment stripping (`key: value  # this is ignored`)
- Empty values (`key:` or `key: ""`) — prints an empty line, exits 0
- stdin via `-` filename
- LF (`\n`) and CRLF (`\r\n`) line endings

### Not supported (v0.1.0)

- Sequences / arrays (`- item`) — exits 4 if encountered on the lookup path
- Multi-document streams (`---`) — treated as a parse error
- Tab-indented files — exits 4
- Anchors and aliases (`&anchor`, `*alias`) — keys containing these characters
  are matched literally if on the path, otherwise silently ignored

### Duplicate keys

The first occurrence wins. The streaming parser stops as soon as the full path
is matched, so later duplicate keys are never reached.

## Benchmarks

Measured on macOS (Apple Silicon) extracting a 3-level nested key from a
realistic 80-key CI/CD configuration file (`bench/fixture.yaml`).
Timings represent full process startup + file parse + output per invocation —
the cost a shell script pays on each call.

| Tool | ms / invocation | vs yamlget |
|------|-----------------|------------|
| **yamlget 0.1.0** | **~1.5 ms** | 1× (baseline) |
| python3 + PyYAML | ~24 ms | ~16× slower |
| python3 + ruamel.yaml | ~30 ms (est.) | ~20× slower |
| yq (Go) | ~5–15 ms (est.) | ~3–10× slower |

> Numbers are indicative. Process startup dominates for small files. Run
> `make bench` to measure on your own hardware. For scripts that call
> `yamlget` hundreds of times, the startup cost compounds — this is where
> the gap vs. Python matters most.

To run the benchmark locally:

```sh
make bench              # 1000 iterations
make bench BENCH_N=500  # 500 iterations
```

## v0.1.0 Scope

- [x] Streaming lexer: blank/comment/key-only/key-value line classification
- [x] Plain, single-quoted, and double-quoted scalar extraction
- [x] Literal block scalars (`|`) and folded block scalars (`>`)
- [x] All chomping indicators (`-`, `+`)
- [x] Indentation depth tracking via pop-based stack (no AST)
- [x] Dot-notation path lookup with sibling-branch isolation
- [x] Exact key matching — no prefix or suffix bleed
- [x] Raw value output to stdout; all diagnostics to stderr (filename + line)
- [x] stdin support via `-` filename
- [x] LF and CRLF line ending support
- [x] 112 tests (10 lexer unit + 102 integration), zero ASan/UBSan errors
- [x] Benchmark suite (`make bench`) comparing yamlget vs python+PyYAML
- [x] Pre-built binaries for Linux x86_64, macOS x86_64, macOS arm64, Windows x86_64
- [ ] Array / sequence indexing — deferred to v0.2.0
- [ ] JSON output (`--json`) — deferred
- [ ] Shell export format (`--export`) — deferred

See [docs/roadmap-v0.1.0.md](docs/roadmap-v0.1.0.md) and [docs/roadmap-future.md](docs/roadmap-future.md).

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). All contributions are welcome.

Key constraint: `yamlget` must remain a **dependency-free pure-C binary**. No new
library dependencies. No new runtime requirements. When in doubt, open an issue
before writing code.

## License

[MIT](LICENSE)
