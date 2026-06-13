# yamlget

A dependency-free YAML key extractor written in pure C.

`yamlget` reads a YAML file and prints the value at a dot-notation path to stdout. Nothing more. It is designed for CI/CD pipelines, shell scripts, containers, and any environment where pulling in Python, Node, or `yq` is too heavy.

```sh
$ yamlget config.yaml database.host
localhost

$ yamlget config.yaml app.version
1.4.2
```

## Why yamlget?

| Tool | Dependency | Binary size | Purpose |
|------|------------|-------------|---------|
| `yq` | Go runtime / binary | ~8 MB | General YAML processor |
| `python -c` | Python interpreter | — | General scripting |
| `node -e` | Node.js runtime | — | General scripting |
| **yamlget** | **None** | **<100 KB** | **Key extraction only** |

`yamlget` does **one thing**: extract a value by path. It does not edit, merge, transform, or validate YAML. That constraint is a feature — it keeps the binary tiny and the behaviour predictable.

## Status

> **Pre-release — M2 complete.** The streaming lexer foundation is implemented and tested. The parser (M3) and lookup (M4) milestones are next. See [docs/roadmap-v0.1.0.md](docs/roadmap-v0.1.0.md) for full scope.

| Milestone | Status |
|-----------|--------|
| M1 — Repository skeleton | ✅ Complete |
| M2 — Streaming lexer | ✅ Complete |
| M3 — Parser | In progress |
| M4 — Lookup | Not started |
| M5 — Integration & polish | Not started |

## Installation

### Build from source

Requirements: a C99-compatible compiler (`gcc`, `clang`, or MSVC).

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
cl /W4 /WX /O2 /D_CRT_SECURE_NO_WARNINGS /Fe:yamlget.exe /Iinclude src\main.c src\lexer.c
```

### Verify the build

```sh
yamlget --version
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
```

```sh
yamlget config.yaml app.name          # myservice
yamlget config.yaml database.port     # 5432
yamlget config.yaml deploy.region     # us-east-1
```

### Reading from stdin

```sh
cat config.yaml | yamlget - app.name
```

### Exit codes

| Code | Meaning |
|------|---------|
| `0` | Success — value printed to stdout |
| `1` | Key not found |
| `2` | Invalid arguments |
| `3` | File error (not found, not readable) |
| `4` | Parse error (invalid YAML) |
| `5` | Internal error |

### Use in CI/CD

```sh
VERSION=$(yamlget Chart.yaml appVersion)
echo "Deploying version $VERSION"
```

```yaml
# GitHub Actions example
- name: Read app version
  run: echo "VERSION=$(yamlget config.yaml app.version)" >> $GITHUB_ENV
```

## v0.1.0 Scope

- [x] Streaming lexer: blank/comment/key-only/key-value line classification
- [x] Plain, single-quoted, and double-quoted scalar extraction
- [x] Indentation depth tracking; tab detection
- [x] Errors to stderr with filename and line number; defined exit codes
- [ ] Parser: indentation-stack descent (M3)
- [ ] Dot-notation path lookup in nested mappings (M4)
- [ ] Raw value output to stdout (M5)
- [ ] stdin support (`-` as filename) (M5)
- [ ] Array indexing — deferred to v0.2.0
- [ ] JSON output (`--json`) — deferred
- [ ] Shell export format (`--export`) — deferred
- [ ] Multi-document YAML — deferred

See [docs/roadmap-v0.1.0.md](docs/roadmap-v0.1.0.md) for full details.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md). All contributions are welcome.

## License

[MIT](LICENSE)
