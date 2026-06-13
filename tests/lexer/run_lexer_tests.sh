#!/usr/bin/env bash
# run_lexer_tests.sh — lexer unit test runner
#
# For each .expected file in fixtures/, runs test_lexer against the matching
# .yaml file and compares the output line-for-line. Lexer diagnostics printed
# to stderr are suppressed; only stdout is compared.
#
# Usage: bash tests/lexer/run_lexer_tests.sh [path/to/test_lexer]
# The test_lexer binary defaults to ./tests/lexer/test_lexer.

set -euo pipefail

BINARY="${1:-./tests/lexer/test_lexer}"
FIXTURES="tests/lexer/fixtures"
PASS=0
FAIL=0
ERRORS=()

if [ ! -x "$BINARY" ]; then
    echo "ERROR: test_lexer binary not found or not executable: $BINARY"
    echo "       Run 'make test-lexer' to build and run, or 'make test-lexer-build' to build only."
    exit 1
fi

echo "Running lexer tests  (binary: $BINARY)"
echo "────────────────────────────────────────"

for expected_file in "$FIXTURES"/*.expected; do
    base="${expected_file%.expected}"
    yaml_file="${base}.yaml"
    name=$(basename "$base")

    if [ ! -f "$yaml_file" ]; then
        echo "WARN  $name: no matching .yaml fixture — skipping"
        continue
    fi

    actual=$("$BINARY" "$yaml_file" 2>/dev/null) || true
    expected=$(cat "$expected_file")

    if [ "$actual" = "$expected" ]; then
        PASS=$((PASS + 1))
        echo "PASS  $name"
    else
        FAIL=$((FAIL + 1))
        ERRORS+=("$name")
        echo "FAIL  $name"
        # Show a compact diff to help diagnose failures.
        diff <(printf '%s\n' "$expected") <(printf '%s\n' "$actual") \
            | sed 's/^/       /' || true
    fi
done

echo "────────────────────────────────────────"
printf "Results: %d passed, %d failed\n" "$PASS" "$FAIL"

if [ "${#ERRORS[@]}" -gt 0 ]; then
    echo ""
    echo "Failed fixtures:"
    for e in "${ERRORS[@]}"; do
        echo "  - $e"
    done
    exit 1
fi
