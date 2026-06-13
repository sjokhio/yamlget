#!/usr/bin/env bash
# run_tests.sh — integration test runner for yamlget
#
# Runs the built binary against test fixtures and checks exit codes and output.
# Add new test cases in the "Tests" section below.
#
# Usage: bash tests/run_tests.sh
# Requirements: yamlget binary must be built (run `make` first).

set -euo pipefail

BINARY="${1:-./yamlget}"
FIXTURES="tests/fixtures"
PASS=0
FAIL=0
ERRORS=()

# ── Helpers ──────────────────────────────────────────────────────────────────

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
        ERRORS+=("FAIL [$description]: expected output '${expected_output}', got '${actual_output}'")
        return
    fi

    PASS=$((PASS + 1))
    echo "PASS  $description"
}

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

# ── Prerequisite ─────────────────────────────────────────────────────────────

if [ ! -x "$BINARY" ]; then
    echo "ERROR: binary not found or not executable: $BINARY"
    echo "       Run 'make' first."
    exit 1
fi

echo "Running yamlget tests (binary: $BINARY)"
echo "────────────────────────────────────────"

# ── Flags ────────────────────────────────────────────────────────────────────

check "--version exits 0"   0 "" --version
check "--help exits 0"      0 "" --help
check "-h exits 0"          0 "" -h
check "-V exits 0"          0 "" -V

# ── Argument validation ───────────────────────────────────────────────────────

check_stderr "no args → exit 2"            2  # no args
check_stderr "one arg → exit 2"            2  "$FIXTURES/simple.yaml"
check_stderr "empty key path → exit 2"     2  "$FIXTURES/simple.yaml" ""
check_stderr "too many args → exit 2"      2  "$FIXTURES/simple.yaml" "name" "extra"

# ── File errors ───────────────────────────────────────────────────────────────

check_stderr "missing file → exit 3"       3  "/nonexistent/path/file.yaml" "name"

# ── simple.yaml — flat scalars ────────────────────────────────────────────────

check "simple: name"           0 "myservice"   "$FIXTURES/simple.yaml" "name"
check "simple: version"        0 "1.4.2"       "$FIXTURES/simple.yaml" "version"
check "simple: port"           0 "8080"        "$FIXTURES/simple.yaml" "port"
check "simple: enabled"        0 "true"        "$FIXTURES/simple.yaml" "enabled"
check "simple: debug"          0 "false"       "$FIXTURES/simple.yaml" "debug"
check "simple: missing key"    1 ""            "$FIXTURES/simple.yaml" "does.not.exist"

# ── nested.yaml — dot-notation traversal ─────────────────────────────────────

check "nested: app.name"                   0 "myservice"    "$FIXTURES/nested.yaml" "app.name"
check "nested: app.version"               0 "2.0.0"        "$FIXTURES/nested.yaml" "app.version"
check "nested: database.host"             0 "localhost"    "$FIXTURES/nested.yaml" "database.host"
check "nested: database.port"             0 "5432"         "$FIXTURES/nested.yaml" "database.port"
check "nested: database.credentials.username" 0 "admin"   "$FIXTURES/nested.yaml" "database.credentials.username"
check "nested: deploy.image.tag"          0 "latest"       "$FIXTURES/nested.yaml" "deploy.image.tag"
check "nested: logging.level"             0 "info"         "$FIXTURES/nested.yaml" "logging.level"
check "nested: missing top-level"         1 ""             "$FIXTURES/nested.yaml" "nonexistent"
check "nested: missing nested"            1 ""             "$FIXTURES/nested.yaml" "app.nonexistent"

# ── ci-pipeline.yaml — realistic fixture ─────────────────────────────────────

check "ci: project.name"           0 "acme-backend" "$FIXTURES/ci-pipeline.yaml" "project.name"
check "ci: project.version"        0 "3.1.4"        "$FIXTURES/ci-pipeline.yaml" "project.version"
check "ci: build.image"            0 "golang:1.22-alpine" "$FIXTURES/ci-pipeline.yaml" "build.image"
check "ci: test.coverage.threshold" 0 "80"          "$FIXTURES/ci-pipeline.yaml" "test.coverage.threshold"
check "ci: docker.tag"             0 "3.1.4"        "$FIXTURES/ci-pipeline.yaml" "docker.tag"
check "ci: deploy.replicas"        0 "5"            "$FIXTURES/ci-pipeline.yaml" "deploy.replicas"

# ── Parse error ───────────────────────────────────────────────────────────────

check_stderr "invalid yaml → exit 4"  4  "$FIXTURES/invalid.yaml" "key"

# ── Summary ───────────────────────────────────────────────────────────────────

echo "────────────────────────────────────────"
echo "Results: $PASS passed, $FAIL failed"

if [ ${#ERRORS[@]} -gt 0 ]; then
    echo ""
    for err in "${ERRORS[@]}"; do
        echo "  $err"
    done
    exit 1
fi
