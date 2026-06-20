# Design: Sequence Lookup (v0.2.0)

This document defines the design for array/sequence index lookup in `yamlget` v0.2.0. It is implementation-oriented and scoped tightly to avoid scope creep from v0.1.0.

---

## Goal

Allow a dot-notation path to include one bracket index per segment, so callers can extract scalar values and nested mapping keys from block sequences.

```sh
yamlget config.yaml servers[0].host
yamlget config.yaml pipeline.steps[2].name
yamlget config.yaml tags[1]
```

---

## Syntax

A path segment may optionally carry a single bracket index:

```
segment      ::= key [ '[' index ']' ]
key          ::= <non-empty string, no dots, no brackets>
index        ::= <non-negative decimal integer, no leading zeros except "0">
path         ::= segment ( '.' segment )*
```

Examples of valid paths:

```
servers[0].host
pipeline.steps[2].name
tags[1]
items[0]
deploy.regions[3].zone
```

Bracket notation is only meaningful when the value at the key is a block sequence. An index on a scalar or mapping key is a path mismatch (exit 1, not found).

---

## Supported examples

```yaml
# tags[1] -> "beta"
tags:
  - alpha
  - beta
  - stable

# servers[0].host -> "db.internal"
servers:
  - host: db.internal
    port: 5432
  - host: cache.internal
    port: 6379

# pipeline.steps[2].name -> "deploy"
pipeline:
  steps:
    - name: build
    - name: test
    - name: deploy

# items[0] -> "first"
items:
  - first
```

---

## Unsupported examples (exit 1 or exit 4)

```yaml
# Negative index: items[-1]         - exit 2 (bad path syntax)
# Slice: items[0:2]                 - exit 2 (bad path syntax)
# Wildcard: items[*]                - exit 2 (bad path syntax)
# Nested sequence: matrix[0][1]     - exit 2 (bad path syntax; only one bracket per segment)
# Flow sequence: tags: [a, b, c]    - exit 4 (parse error; flow sequences not supported)
# Flow mapping item:
#   servers:
#     - {host: x, port: 1}          - exit 4 (parse error; flow mappings not supported)
# Anchor/alias in sequence          - exit 4 (parse error, same as v0.1.0 behavior)
```

---

## Path parsing changes

### Current behavior

`yg_path_split()` splits on `.` and stores each segment as a plain string in a `char[YG_KEY_MAX]` slot.

### Required change

Each segment needs to carry an optional index. Extend `yg_path_seg_t` (new type) to hold both:

```c
#define YG_SEG_NO_INDEX  (-1)

typedef struct {
    char key[YG_KEY_MAX];
    int  index;          /* >= 0 if bracket present, YG_SEG_NO_INDEX otherwise */
} yg_path_seg_t;
```

Replace the current `char segs[][YG_KEY_MAX]` array with `yg_path_seg_t segs[]`.

`yg_path_split()` signature change:

```c
/* v0.1.0 */
int yg_path_split(const char *path, char segs[][YG_KEY_MAX], int max);

/* v0.2.0 */
int yg_path_split(const char *path, yg_path_seg_t *segs, int max);
```

Parsing logic for each segment after splitting on `.`:

1. Scan for `[`. If absent, copy to `seg.key`, set `seg.index = YG_SEG_NO_INDEX`.
2. If present, everything before `[` is `seg.key`; everything between `[` and `]` is parsed as a decimal integer.
3. Validation failures (empty key, no closing `]`, non-decimal chars, leading zeros on multi-digit number, index > INT_MAX) return -1 from `yg_path_split()` -> exit 2.
4. A bare `[]` (empty index) is a validation error -> exit 2.

No dynamic allocation. All storage fits in the existing fixed-depth `yg_path_seg_t segs[YG_PATH_MAX_DEPTH]` on the stack in `main()`.

---

## Lexer impact

The lexer does not need new line types. Block sequence items (`- value`) already produce `INVALID` in v0.1.0 because they have no colon. Two new line types are needed:

### New line types

```c
YG_LINE_SEQ_SCALAR   /* "  - value"   : a sequence item with a plain scalar value */
YG_LINE_SEQ_MAPPING  /* "  - key: v"  : a sequence item that begins a mapping      */
YG_LINE_SEQ_EMPTY    /* "  -"         : a sequence item with null/empty value       */
```

Detection in `yg_lexer_next()`: after stripping leading spaces, if the line matches `^- ` or `^-\s*$`:

- Record `line.indent` as the column of the `-` (same leading-space counting as today).
- For `SEQ_SCALAR`: content after `- ` is the value (same scalar extraction rules as KEY_VALUE: plain, single-quoted, double-quoted, inline comment stripping).
- For `SEQ_MAPPING`: content after `- ` is treated as a `key: value` or `key:` pair inline; store key and value in `line.key` / `line.value`. The mapping continuation lines (subsequent indented lines) are standard KEY_ONLY / KEY_VALUE lines at a deeper indent, handled by the existing parser stack.
- For `SEQ_EMPTY`: `line.value` is empty, `line.has_value = 0`.

Block scalar sequence items (`- |`, `- >`) are deferred. If the lexer sees `- |` or `- >`, emit `INVALID` (exit 4). This is consistent with deferring block scalars as sequence items.

### Indent convention

The `-` character occupies the indent column. The child content of the sequence item (whether a scalar inline or a mapping key) is conceptually indented by at least `indent + 2` (dash + space). The parser uses this to detect when a sequence ends.

---

## Parser impact

### Current stack frame

The parser maintains an indentation stack to track matched vs. blocked nesting. Each frame records:

- `indent` - the indentation level of the matched key
- `matched` - whether this frame is on the active lookup path

### New state needed per frame

```c
typedef enum {
    YG_FRAME_MAPPING,   /* current context is a block mapping (default) */
    YG_FRAME_SEQUENCE,  /* current context is a block sequence          */
} yg_frame_kind_t;

typedef struct {
    int              indent;
    int              matched;
    yg_frame_kind_t  kind;
    int              seq_current;  /* item counter, incremented per SEQ_* line */
    int              seq_target;   /* index we are looking for, or -1          */
} yg_stack_frame_t;
```

`seq_current` starts at 0 and increments each time a `SEQ_*` line is seen at the correct indent while inside a `YG_FRAME_SEQUENCE` frame. When `seq_current == seq_target` the item is the one we want.

### State machine changes

When the parser pops to a frame where the active segment has `index >= 0`:

1. After matching the key, instead of expecting a scalar value, push a new `YG_FRAME_SEQUENCE` frame with `seq_target = seg.index`, `seq_current = 0`, `matched = 1`.

When a `SEQ_*` line is seen and the top frame is `YG_FRAME_SEQUENCE` and `matched`:

- If `seq_current < seq_target`: increment `seq_current`, skip item.
- If `seq_current == seq_target`:
  - For `SEQ_SCALAR`: value is in `line.value`. If this is the final path segment, print and exit 0. Otherwise exit 1 (cannot descend into a scalar).
  - For `SEQ_EMPTY`: if final segment, print empty and exit 0. Otherwise exit 1.
  - For `SEQ_MAPPING`: if this is the final path segment, exit 1 (no scalar to print). Otherwise push a `YG_FRAME_MAPPING` frame to continue resolving the next path segment against the inline key and subsequent lines.
- If `seq_current > seq_target`: item was passed; the index does not exist in this sequence -> exit 1 (not found). This is detected by pop: when a line with indent <= the sequence frame's indent appears, pop and conclude not found.

When a non-`SEQ_*` line appears while inside a `YG_FRAME_SEQUENCE` frame and `seq_current < seq_target`: the sequence ended before the target index was reached -> exit 1.

### No-allocation constraint

`seq_current` and `seq_target` are plain `int` fields on the existing stack frame struct, which is already stack-allocated. No heap allocation needed.

---

## Exit-code behavior

| Situation | Exit code |
|-----------|:---------:|
| Index found, scalar printed | 0 |
| Key exists but index out of range | 1 |
| Key exists but is not a sequence (index given) | 1 |
| Index path valid but key not found | 1 |
| Bad bracket syntax in path (`[`, `[-1]`, `[*]`, `[]`) | 2 |
| Flow sequence encountered | 4 |
| Flow mapping item in sequence | 4 |
| Block scalar sequence item (`- |`) | 4 |
| Tab indentation | 4 (unchanged) |

Rationale for exit 1 (not exit 4) on type mismatch: the file is valid YAML within the supported subset; the path simply does not resolve to a value. This is consistent with how v0.1.0 handles a key that exists but whose value is a mapping when a scalar is expected.

---

## Test plan

### Lexer unit tests

New fixture `tests/lexer/fixtures/sequences.yaml` + `.expected` covering:

- Flat scalar sequence (`- alpha`, `- beta`)
- Sequence of mappings (`- key: val`)
- Mixed indent widths (2-space, 4-space)
- Sequence item with empty value (`-`)
- Sequence preceded and followed by mapping keys (put-back correctness)

### Integration tests (new fixtures)

`tests/fixtures/sequences.yaml` - comprehensive block sequence file:

```yaml
tags:
  - alpha
  - beta
  - stable

servers:
  - host: db.internal
    port: 5432
  - host: cache.internal
    port: 6379

pipeline:
  steps:
    - name: build
      image: golang:1.22
    - name: test
      image: golang:1.22
    - name: deploy
      image: alpine:3

empty_seq:
  -
  - second
```

Test cases:

| Description | Path | Expected exit | Expected output |
|-------------|------|:-------------:|-----------------|
| Scalar index 0 | `tags[0]` | 0 | `alpha` |
| Scalar index 2 | `tags[2]` | 0 | `stable` |
| Index out of range | `tags[5]` | 1 | |
| Mapping item scalar | `servers[0].host` | 0 | `db.internal` |
| Mapping item scalar, second item | `servers[1].port` | 0 | `6379` |
| 3-level: seq in nested mapping | `pipeline.steps[2].name` | 0 | `deploy` |
| 3-level: deeper key in seq item | `pipeline.steps[0].image` | 0 | `golang:1.22` |
| Key not found in seq item | `servers[0].nonexistent` | 1 | |
| Index on scalar value | `pipeline.steps[0].name[0]` | 1 | |
| Sequence key without index | `tags` | 1 (no scalar) | |
| Empty seq item | `empty_seq[0]` | 0 | `` (empty) |
| No-index path, no-seq key | `servers[0]` (final seg) | 1 (mapping, not scalar) | |
| Flow sequence | (fixture with `tags: [a, b]`) | 4 | |

### Exit code tests

- `yamlget sequences.yaml tags[-1]` -> exit 2
- `yamlget sequences.yaml tags[*]` -> exit 2
- `yamlget sequences.yaml tags[]` -> exit 2
- `yamlget sequences.yaml tags[0][1]` -> exit 2 (nested brackets)

### Edge cases

- Index 0 on a one-item sequence: exit 0
- Sequence immediately under root (indent 0): verify stack handles `seq_indent = 0`
- Sequence item followed by a sibling key at the same level as the sequence key
- Deep nesting: mapping -> sequence -> mapping -> sequence (deferred per scope, but verify exit 4 or 1, not crash)

---

## Compatibility notes

### v0.1.0 paths remain unchanged

Paths without bracket syntax follow exactly the same code path as today. `yg_path_seg_t` with `index = YG_SEG_NO_INDEX` is handled identically to the current `char[]` segment. No behavior change for existing valid paths.

### Sequences currently exit 4

In v0.1.0, a `- item` line produces `YG_LINE_INVALID`, which causes `yg_stream_lookup()` to return `YAMLGET_EXIT_PARSE_ERROR`. In v0.2.0:

- If the sequence is **on the active lookup path** and the path segment has an index, it is resolved normally.
- If the sequence is **on the active lookup path** and the path segment has **no index**, it is still a mismatch: exit 1 (the key resolves to a sequence, not a scalar).
- If the sequence is **off the active lookup path** (a sibling branch), the `SEQ_*` lines should be skipped rather than triggering exit 4. This requires the parser to suppress `INVALID`-from-sequence in blocked frames, distinguishing `YG_LINE_INVALID` (true parse error, tab indent etc.) from `YG_LINE_SEQ_*` (valid YAML, just not on path).

This is a **compatibility improvement**: YAML files that contain sequences outside the lookup path will no longer spuriously exit 4.

### `yg_path_split()` signature change

The parameter type changes from `char[][YG_KEY_MAX]` to `yg_path_seg_t *`. This is an internal API (not part of the public `include/yamlget.h` header) so it does not affect library consumers. `main.c` and any caller must be updated to use the new type.

---

## Out of scope for v0.2.0

| Feature | Reason |
|---------|--------|
| Negative indexes (`[-1]`) | Requires knowing sequence length before streaming |
| Slices (`[0:2]`) | Non-trivial output format; deferred to output-format milestone |
| Wildcards (`[*]`) | Non-trivial multi-value output; deferred |
| Filter expressions | Out of scope for a key extractor |
| Flow sequences (`[a, b, c]`) | Requires inline tokenizer; deferred |
| Flow mapping items (`- {k: v}`) | Same as above |
| Nested brackets (`matrix[0][1]`) | One bracket per segment is sufficient for common cases |
| Block scalars as sequence items | Interaction with `buf_pending` lookahead adds complexity |
| Anchors and aliases | Unchanged from v0.1.0 |
| Multi-document streams | Unchanged from v0.1.0 |
