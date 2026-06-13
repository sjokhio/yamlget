# Future Roadmap

This document tracks ideas and feature requests that are out of scope for v0.1.0 but may be considered for future releases. Nothing here is committed. Priority and sequencing will be determined based on community feedback and maintainer capacity.

Items are grouped by release candidate. All are subject to change.

---

## v0.2.0 — Sequences

**Theme:** Make `yamlget` useful for YAML files that contain lists.

- Array indexing by integer: `servers.0.host`, `items.2`
- Negative indexing: `items.-1` (last element)
- Tests and fixtures for all sequence types

**Not included:** filtering, slicing, wildcard matching.

---

## v0.3.0 — Output formats

**Theme:** Make output consumable by more tooling without a post-processing step.

- `--raw` (default, current behaviour): plain scalar value
- `--json`: emit the value as a JSON primitive (`"string"`, `123`, `true`, `null`)
- `--export`: emit `KEY=VALUE` suitable for shell `eval` / `export`
- `--null-on-missing`: print `null` and exit 0 when key not found (instead of exit 1)

---

## v0.4.0 — Anchors and aliases

**Theme:** Handle the most common advanced YAML features found in real configuration files.

- `&anchor` / `*alias` resolution (dereference at parse time)
- `<<: *defaults` merge keys

**Not included:** tag processing, custom constructors.

---

## v0.5.0 — Multiple inputs and globbing

**Theme:** Reduce boilerplate in shell scripts that process many files.

- Multiple file arguments: `yamlget *.yaml app.version`
- Print `filename: value` per match when multiple files given
- `--first` flag: stop after the first file that contains the key

---

## v1.0.0 — Stability milestone

Signals that the interface is stable and v0.x users can upgrade without breaking changes.

- All v0.x features stabilised
- Man page complete
- Comprehensive test suite (>95% line coverage)
- Fuzz testing (libFuzzer or AFL) integrated into CI
- Package manager presence: Homebrew, APT PPA or Debian package, Chocolatey
- Semantic versioning guarantee: no breaking CLI changes without a major version bump

---

## Deferred indefinitely

These have been discussed and explicitly deferred. Reopen the conversation with a concrete use case before submitting a PR.

| Feature | Reason deferred |
|---------|----------------|
| YAML editing / rewriting | Out of scope — use `yq` or a language library |
| Schema validation | Out of scope — not a key extractor concern |
| Multi-document stream queries | Complex; defer until demand is clear |
| YAML → TOML / JSON conversion | Out of scope |
| Recursive / wildcard path matching | Adds significant complexity; assess after v1.0 |
| Plugin system | Premature abstraction |
| Embedded library mode (`libyamlget`) | Possible post-v1.0 if demand warrants |

---

## How to propose a feature

1. Open a GitHub issue with the label `feature request`.
2. Describe the **use case**, not just the feature. What problem does it solve? Why can't you solve it with `yamlget` today?
3. If the feature is in scope, it will be assigned to a milestone here. If it is out of scope, the issue will be closed with an explanation.
