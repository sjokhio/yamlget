#!/usr/bin/env bash
# run_tests.sh — integration test runner for yamlget
#
# Runs the built binary against test fixtures and checks exit codes and output.
# Stderr is always suppressed; only stdout and exit codes are tested.
#
# Usage: bash tests/run_tests.sh [path/to/yamlget]

set -euo pipefail

BINARY="${1:-./yamlget}"
# MinGW/MSYS2 on Windows: gcc produces yamlget.exe; find it when the bare name is absent.
if [ ! -x "$BINARY" ] && [ -x "${BINARY}.exe" ]; then
    BINARY="${BINARY}.exe"
fi
FIXTURES="tests/fixtures"
PASS=0
FAIL=0
ERRORS=()

# ── Helpers ───────────────────────────────────────────────────────────────────

# check <desc> <expected_exit> <expected_output> [args...]
#   Runs $BINARY with [args...], compares exit code and (if non-empty) stdout.
check() {
    local description="$1"
    local expected_exit="$2"
    local expected_output="$3"
    shift 3
    local actual_output actual_exit

    actual_output=$("$BINARY" "$@" 2>/dev/null) && actual_exit=$? || actual_exit=$?

    if [ "$actual_exit" -ne "$expected_exit" ]; then
        FAIL=$((FAIL + 1))
        ERRORS+=("FAIL [$description]: expected exit $expected_exit, got $actual_exit")
        return
    fi

    if [ -n "$expected_output" ] && [ "$actual_output" != "$expected_output" ]; then
        FAIL=$((FAIL + 1))
        ERRORS+=("FAIL [$description]: expected '${expected_output}', got '${actual_output}'")
        return
    fi

    PASS=$((PASS + 1))
    echo "PASS  $description"
}

# check_exact <desc> <expected_exit> <expected_output> [args...]
#   Like check but always compares stdout, even when expected_output is empty.
check_exact() {
    local description="$1"
    local expected_exit="$2"
    local expected_output="$3"
    shift 3
    local actual_output actual_exit

    actual_output=$("$BINARY" "$@" 2>/dev/null) && actual_exit=$? || actual_exit=$?

    if [ "$actual_exit" -ne "$expected_exit" ]; then
        FAIL=$((FAIL + 1))
        ERRORS+=("FAIL [$description]: expected exit $expected_exit, got $actual_exit")
        return
    fi

    if [ "$actual_output" != "$expected_output" ]; then
        FAIL=$((FAIL + 1))
        ERRORS+=("FAIL [$description]: expected '${expected_output}', got '${actual_output}'")
        return
    fi

    PASS=$((PASS + 1))
    echo "PASS  $description"
}

# check_stderr <desc> <expected_exit> [args...]
#   Only checks the exit code; stdout and stderr are both suppressed.
check_stderr() {
    local description="$1"
    local expected_exit="$2"
    shift 2
    local actual_exit

    "$BINARY" "$@" >/dev/null 2>/dev/null && actual_exit=$? || actual_exit=$?

    if [ "$actual_exit" -ne "$expected_exit" ]; then
        FAIL=$((FAIL + 1))
        ERRORS+=("FAIL [$description]: expected exit $expected_exit, got $actual_exit")
        return
    fi

    PASS=$((PASS + 1))
    echo "PASS  $description"
}

# check_stdin <desc> <expected_exit> <expected_output> <yaml_file> <keypath>
#   Pipes yaml_file to stdin and runs "$BINARY - keypath".
check_stdin() {
    local description="$1"
    local expected_exit="$2"
    local expected_output="$3"
    local yaml_file="$4"
    local keypath="$5"
    local actual_output actual_exit

    actual_output=$(cat "$yaml_file" | "$BINARY" - "$keypath" 2>/dev/null) \
        && actual_exit=$? || actual_exit=$?

    if [ "$actual_exit" -ne "$expected_exit" ]; then
        FAIL=$((FAIL + 1))
        ERRORS+=("FAIL [$description]: expected exit $expected_exit, got $actual_exit")
        return
    fi

    if [ -n "$expected_output" ] && [ "$actual_output" != "$expected_output" ]; then
        FAIL=$((FAIL + 1))
        ERRORS+=("FAIL [$description]: expected '${expected_output}', got '${actual_output}'")
        return
    fi

    PASS=$((PASS + 1))
    echo "PASS  $description"
}

# ── Prerequisite ──────────────────────────────────────────────────────────────

if [ ! -x "$BINARY" ]; then
    echo "ERROR: binary not found or not executable: $BINARY"
    echo "       Run 'make' first."
    exit 1
fi

echo "Running yamlget tests  (binary: $BINARY)"
echo "────────────────────────────────────────"

# ── Flags ─────────────────────────────────────────────────────────────────────

check "--version exits 0"        0 "" --version
check "--help exits 0"           0 "" --help
check "-h exits 0"               0 "" -h
check "-V exits 0"               0 "" -V
check "--version output"         0 "0.1.0" --version

# ── Argument validation ───────────────────────────────────────────────────────

check_stderr "no args → exit 2"                  2
check_stderr "one arg → exit 2"                  2  "$FIXTURES/simple.yaml"
check_stderr "empty key path → exit 2"           2  "$FIXTURES/simple.yaml" ""
check_stderr "too many args → exit 2"            2  "$FIXTURES/simple.yaml" "name" "extra"
check_stderr "leading dot in path → exit 2"      2  "$FIXTURES/simple.yaml" ".name"
check_stderr "trailing dot in path → exit 2"     2  "$FIXTURES/simple.yaml" "name."
check_stderr "double dot in path → exit 2"       2  "$FIXTURES/simple.yaml" "app..name"

# ── File errors ───────────────────────────────────────────────────────────────

check_stderr "missing file → exit 3"             3  "/nonexistent/path/file.yaml" "name"

# ── stdin support ─────────────────────────────────────────────────────────────

check_stdin "stdin: flat key"                    0 "myservice"      "$FIXTURES/simple.yaml"       "name"
check_stdin "stdin: nested key"                  0 "localhost"      "$FIXTURES/nested.yaml"       "database.host"
check_stdin "stdin: not found"                   1 ""               "$FIXTURES/simple.yaml"       "nonexistent"

# ── simple.yaml — flat scalars ────────────────────────────────────────────────

check "simple: name"                             0 "myservice"      "$FIXTURES/simple.yaml" "name"
check "simple: version"                          0 "1.4.2"          "$FIXTURES/simple.yaml" "version"
check "simple: port"                             0 "8080"           "$FIXTURES/simple.yaml" "port"
check "simple: enabled"                          0 "true"           "$FIXTURES/simple.yaml" "enabled"
check "simple: debug"                            0 "false"          "$FIXTURES/simple.yaml" "debug"
check "simple: ratio"                            0 "0.75"           "$FIXTURES/simple.yaml" "ratio"
check "simple: nothing (tilde null)"             0 "~"              "$FIXTURES/simple.yaml" "nothing"
check_stderr "simple: empty value → exit 0"      0                  "$FIXTURES/simple.yaml" "empty"
check "simple: missing key → exit 1"             1 ""               "$FIXTURES/simple.yaml" "does.not.exist"
check "simple: missing nested → exit 1"          1 ""               "$FIXTURES/simple.yaml" "name.child"

# ── nested.yaml — dot-notation traversal ─────────────────────────────────────

check "nested: app.name"                         0 "myservice"      "$FIXTURES/nested.yaml" "app.name"
check "nested: app.version"                      0 "2.0.0"          "$FIXTURES/nested.yaml" "app.version"
check "nested: app.description (double-quoted)"  0 "A sample service for yamlget tests" \
                                                                     "$FIXTURES/nested.yaml" "app.description"
check "nested: database.host"                    0 "localhost"      "$FIXTURES/nested.yaml" "database.host"
check "nested: database.port"                    0 "5432"           "$FIXTURES/nested.yaml" "database.port"
check "nested: database.name"                    0 "mydb"           "$FIXTURES/nested.yaml" "database.name"
check "nested: database.credentials.username"    0 "admin"          "$FIXTURES/nested.yaml" "database.credentials.username"
check "nested: database.credentials.password"    0 "s3cr3t"         "$FIXTURES/nested.yaml" "database.credentials.password"
check "nested: server.http.host"                 0 "0.0.0.0"        "$FIXTURES/nested.yaml" "server.http.host"
check "nested: server.http.port"                 0 "8080"           "$FIXTURES/nested.yaml" "server.http.port"
check "nested: server.grpc.port"                 0 "9090"           "$FIXTURES/nested.yaml" "server.grpc.port"
check "nested: deploy.image.tag"                 0 "latest"         "$FIXTURES/nested.yaml" "deploy.image.tag"
check "nested: deploy.image.pullPolicy"          0 "IfNotPresent"   "$FIXTURES/nested.yaml" "deploy.image.pullPolicy"
check "nested: logging.level"                    0 "info"           "$FIXTURES/nested.yaml" "logging.level"
check "nested: logging.output.rotate"            0 "true"           "$FIXTURES/nested.yaml" "logging.output.rotate"
check "nested: missing top-level → exit 1"       1 ""               "$FIXTURES/nested.yaml" "nonexistent"
check "nested: missing nested → exit 1"          1 ""               "$FIXTURES/nested.yaml" "app.nonexistent"
check "nested: wrong branch → exit 1"            1 ""               "$FIXTURES/nested.yaml" "database.http.port"

# ── exact-match.yaml — no prefix/suffix bleed ────────────────────────────────

check "exact: service.name"                      0 "correct"        "$FIXTURES/exact-match.yaml" "service.name"
check "exact: service.name_suffix"               0 "wrong_suffix"   "$FIXTURES/exact-match.yaml" "service.name_suffix"
check "exact: service.names"                     0 "wrong_plural"   "$FIXTURES/exact-match.yaml" "service.names"
check "exact: service.rename"                    0 "wrong_rename"   "$FIXTURES/exact-match.yaml" "service.rename"
check "exact: database.name"                     0 "db_name"        "$FIXTURES/exact-match.yaml" "database.name"
check "exact: database.namespace"                0 "db_namespace"   "$FIXTURES/exact-match.yaml" "database.namespace"
check "exact: deploy.image.name"                 0 "image_name"     "$FIXTURES/exact-match.yaml" "deploy.image.name"
check "exact: deploy.image.name_tag"             0 "image_tag"      "$FIXTURES/exact-match.yaml" "deploy.image.name_tag"
check "exact: other_service.name"                0 "other_name"     "$FIXTURES/exact-match.yaml" "other_service.name"

# ── ci-pipeline.yaml — realistic multi-level fixture ─────────────────────────

check "ci: project.name"                         0 "acme-backend"          "$FIXTURES/ci-pipeline.yaml" "project.name"
check "ci: project.version"                      0 "3.1.4"                 "$FIXTURES/ci-pipeline.yaml" "project.version"
check "ci: project.language"                     0 "go"                    "$FIXTURES/ci-pipeline.yaml" "project.language"
check "ci: build.image"                          0 "golang:1.22-alpine"    "$FIXTURES/ci-pipeline.yaml" "build.image"
check "ci: build.cache.key"                      0 "go-modules"            "$FIXTURES/ci-pipeline.yaml" "build.cache.key"
check "ci: test.coverage.threshold"              0 "80"                    "$FIXTURES/ci-pipeline.yaml" "test.coverage.threshold"
check "ci: lint.tool"                            0 "golangci-lint"         "$FIXTURES/ci-pipeline.yaml" "lint.tool"
check "ci: docker.registry"                      0 "ghcr.io"               "$FIXTURES/ci-pipeline.yaml" "docker.registry"
check "ci: docker.tag"                           0 "3.1.4"                 "$FIXTURES/ci-pipeline.yaml" "docker.tag"
check "ci: deploy.replicas"                      0 "5"                     "$FIXTURES/ci-pipeline.yaml" "deploy.replicas"
check "ci: deploy.rolling_update.max_surge"      0 "1"                     "$FIXTURES/ci-pipeline.yaml" "deploy.rolling_update.max_surge"
check "ci: deploy.rolling_update.max_unavailable" 0 "0"                    "$FIXTURES/ci-pipeline.yaml" "deploy.rolling_update.max_unavailable"
check "ci: deploy.health_check.path"             0 "/healthz"              "$FIXTURES/ci-pipeline.yaml" "deploy.health_check.path"
check "ci: notifications.slack.channel"          0 "#deployments"          "$FIXTURES/ci-pipeline.yaml" "notifications.slack.channel"
check "ci: missing key → exit 1"                 1 ""                      "$FIXTURES/ci-pipeline.yaml" "deploy.nonexistent"

# ── Parse errors ──────────────────────────────────────────────────────────────

check_stderr "sequence items → exit 4"            4  "$FIXTURES/invalid.yaml" "name"
check_stderr "sequence: descend → exit 4"         4  "$FIXTURES/invalid.yaml" "items.child"

# ── block-scalars.yaml — literal and folded block scalars ────────────────────

check "block: script (literal)"                  0 $'echo hello\necho world' \
    "$FIXTURES/block-scalars.yaml" "script"

check "block: multiline_literal (3 lines)"       0 $'first line\nsecond line\nthird line' \
    "$FIXTURES/block-scalars.yaml" "multiline_literal"

check "block: single_line_literal"               0 "only one line" \
    "$FIXTURES/block-scalars.yaml" "single_line_literal"

check "block: description (folded)"              0 \
    "This is a long description that has been folded across multiple lines." \
    "$FIXTURES/block-scalars.yaml" "description"

check "block: folded_single"                     0 "just one line" \
    "$FIXTURES/block-scalars.yaml" "folded_single"

check "block: stripped (|-)"                     0 $'no trailing newline\nfrom chomping' \
    "$FIXTURES/block-scalars.yaml" "stripped"

check "block: kept (|+)"                         0 "with keep chomping" \
    "$FIXTURES/block-scalars.yaml" "kept"

check_exact "block: empty_block (|)"             0 "" \
    "$FIXTURES/block-scalars.yaml" "empty_block"

check "block: next_after_empty (put-back)"       0 "found" \
    "$FIXTURES/block-scalars.yaml" "next_after_empty"

check "block: nested.literal"                    0 $'line one\nline two' \
    "$FIXTURES/block-scalars.yaml" "nested.literal"

check "block: nested.plain after literal"        0 "simple_value" \
    "$FIXTURES/block-scalars.yaml" "nested.plain"

check "block: nested.folded"                     0 "folded nested value here" \
    "$FIXTURES/block-scalars.yaml" "nested.folded"

check "block: folded_more_indent"                0 $'preamble line\n  more indented keeps newlines\nresuming normal' \
    "$FIXTURES/block-scalars.yaml" "folded_more_indent"

check "block: first_key (literal)"               0 "block content" \
    "$FIXTURES/block-scalars.yaml" "first_key"

check "block: second_key (after block)"          0 "sibling_value" \
    "$FIXTURES/block-scalars.yaml" "second_key"

# ── edge-cases.yaml — quoted scalars, block scalars, deep nesting ─────────────

check "edge: single_quoted"                      0 "hello world"   "$FIXTURES/edge-cases.yaml" "single_quoted"
check "edge: double_quoted"                      0 "hello world"   "$FIXTURES/edge-cases.yaml" "double_quoted"

check "edge: literal_block (3 lines)"            0 $'line one\nline two\nline three' \
    "$FIXTURES/edge-cases.yaml" "literal_block"

check "edge: folded_block (single line)"         0 \
    "this is a long sentence that wraps across lines but becomes one line" \
    "$FIXTURES/edge-cases.yaml" "folded_block"

check "edge: deep nesting (a.b.c.d.e.value)"     0 "deep" \
    "$FIXTURES/edge-cases.yaml" "a.b.c.d.e.value"

# ── crlf.yaml — Windows line endings ─────────────────────────────────────────

check "crlf: name"                               0 "myservice"     "$FIXTURES/crlf.yaml" "name"
check "crlf: version"                            0 "1.4.2"         "$FIXTURES/crlf.yaml" "version"
check "crlf: port"                               0 "8080"          "$FIXTURES/crlf.yaml" "port"
check "crlf: enabled"                            0 "true"          "$FIXTURES/crlf.yaml" "enabled"
check "crlf: missing key → exit 1"              1 ""              "$FIXTURES/crlf.yaml" "nonexistent"

# ── large.yaml — 1001 keys; correctness under scale ──────────────────────────

check "large: find first key"                    0 "value_1"       "$FIXTURES/large.yaml" "key_1"
check "large: find middle key (500)"             0 "value_500"     "$FIXTURES/large.yaml" "key_500"
check "large: find last numbered key (1000)"     0 "value_1000"    "$FIXTURES/large.yaml" "key_1000"
check "large: find sentinel key at end"          0 "found_it"      "$FIXTURES/large.yaml" "final_key"
check "large: missing key → exit 1"             1 ""              "$FIXTURES/large.yaml" "nonexistent"

# ── Malformed inputs — parse errors ──────────────────────────────────────────

check_stderr "malformed: tab in block body → exit 4"   4  "$FIXTURES/malformed-block.yaml" "key"
check_stderr "malformed: tab in block body (miss) → 4" 4  "$FIXTURES/malformed-block.yaml" "next"

# ── Path syntax: bracket index validation (exit 2) ───────────────────────────
# These tests exercise yg_path_split() rejection logic. The YAML file
# argument is irrelevant — path validation happens before file open.

check_stderr "path: empty brackets key[]"           2  "$FIXTURES/simple.yaml" "key[]"
check_stderr "path: negative index key[-1]"         2  "$FIXTURES/simple.yaml" "key[-1]"
check_stderr "path: alpha index key[abc]"           2  "$FIXTURES/simple.yaml" "key[abc]"
check_stderr "path: wildcard key[*]"                2  "$FIXTURES/simple.yaml" "key[*]"
check_stderr "path: nested brackets key[0][1]"      2  "$FIXTURES/simple.yaml" "key[0][1]"
check_stderr "path: unclosed bracket key[0"         2  "$FIXTURES/simple.yaml" "key[0"
check_stderr "path: leading zero key[01]"           2  "$FIXTURES/simple.yaml" "key[01]"
check_stderr "path: no key before bracket [0]"      2  "$FIXTURES/simple.yaml" "[0]"
check_stderr "path: bracket in middle segment a.[0].b" 2 "$FIXTURES/simple.yaml" "a.[0].b"
check_stderr "path: trailing dot after bracket a[0]."  2 "$FIXTURES/simple.yaml" "a[0]."

# Valid bracket syntax parses successfully; lookup result depends on content.
# These confirm that syntactically valid bracket paths reach the lookup stage
# and never exit 2 from path syntax rejection.
# (Index semantics are not yet enforced; the parser matches on key name only.)
check_stderr "path: valid key[0] parses OK (exits 0, key found)" \
                                                    0  "$FIXTURES/simple.yaml" "name[0]"
check_stderr "path: valid key[0] parses OK (exits 1, key missing)" \
                                                    1  "$FIXTURES/simple.yaml" "nonexistent[0]"

# ── Summary ───────────────────────────────────────────────────────────────────

echo "────────────────────────────────────────"
printf "Results: %d passed, %d failed\n" "$PASS" "$FAIL"

if [ "${#ERRORS[@]}" -gt 0 ]; then
    echo ""
    for err in "${ERRORS[@]}"; do
        echo "  $err"
    done
    exit 1
fi
