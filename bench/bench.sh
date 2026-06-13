#!/usr/bin/env bash
# bench.sh — compare yamlget, yq, and python+PyYAML on a shared fixture.
#
# Usage:
#   bash bench/bench.sh             # 1000 iterations per tool
#   bash bench/bench.sh 500         # 500 iterations
#
# Tools not installed are skipped with a notice rather than aborting.
#
# Methodology
# ───────────
# Each tool is run N times in a tight shell loop. The total wall-clock time for
# the loop is measured with the shell's `time` built-in and reported as ms/op.
# All tools extract the same key (metrics.requests.total) from bench/fixture.yaml.
# Output is suppressed (/dev/null) so I/O is not the bottleneck.
#
# Notes:
#   - Process startup dominates for small files. This benchmark measures the
#     full end-to-end cost a shell script pays per invocation.
#   - Results depend heavily on OS caches (warm vs cold), OS scheduler, and
#     hardware. Run multiple times and average for stability.
#   - yq and python results are indicative; exact versions affect timings.

set -euo pipefail

ITERATIONS="${1:-1000}"
FIXTURE="bench/fixture.yaml"
KEY="metrics.requests.total"
YAMLGET="${YAMLGET:-./yamlget}"

if [ ! -f "$FIXTURE" ]; then
    echo "ERROR: fixture not found: $FIXTURE"
    echo "       Run from the repository root: bash bench/bench.sh"
    exit 1
fi

if [ ! -x "$YAMLGET" ]; then
    echo "ERROR: yamlget binary not found: $YAMLGET"
    echo "       Run 'make' first."
    exit 1
fi

echo "yamlget benchmark"
echo "═══════════════════════════════════════════"
echo "  fixture:    $FIXTURE"
echo "  key:        $KEY"
echo "  iterations: $ITERATIONS"
echo ""

# ── Helper: time a command loop ───────────────────────────────────────────────

run_bench() {
    local name="$1"
    local cmd="$2"
    local n="$ITERATIONS"
    local start end elapsed_ms ms_per_op

    # Warm up the OS page cache.
    eval "$cmd" > /dev/null 2>&1 || true

    start=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")

    local i=0
    while [ "$i" -lt "$n" ]; do
        eval "$cmd" > /dev/null 2>&1
        i=$((i + 1))
    done

    end=$(date +%s%N 2>/dev/null || python3 -c "import time; print(int(time.time()*1e9))")

    elapsed_ms=$(( (end - start) / 1000000 ))
    ms_per_op=$(echo "scale=3; $elapsed_ms / $n" | bc 2>/dev/null || python3 -c "print(f'{$elapsed_ms/$n:.3f}')")

    printf "  %-22s  %6d ms total  %8s ms/op\n" "$name" "$elapsed_ms" "$ms_per_op"
}

# ── yamlget ───────────────────────────────────────────────────────────────────

echo "Results:"
run_bench "yamlget" "$YAMLGET '$FIXTURE' '$KEY'"

# ── yq ────────────────────────────────────────────────────────────────────────

if command -v yq > /dev/null 2>&1; then
    YQ_VERSION=$(yq --version 2>&1 | head -1)
    run_bench "yq  ($YQ_VERSION)" "yq e '.$KEY' '$FIXTURE'"
else
    printf "  %-22s  (not installed — skip)\n" "yq"
fi

# ── python + PyYAML ───────────────────────────────────────────────────────────

if python3 -c "import yaml" 2>/dev/null; then
    PY_CMD="python3 -c \"
import yaml, sys
with open('$FIXTURE') as f:
    d = yaml.safe_load(f)
keys = '$KEY'.split('.')
for k in keys:
    d = d[k]
print(d)
\""
    run_bench "python3+PyYAML" "$PY_CMD"
else
    printf "  %-22s  (PyYAML not installed — skip)\n" "python3+PyYAML"
fi

# ── python + ruamel.yaml ──────────────────────────────────────────────────────

if python3 -c "import ruamel.yaml" 2>/dev/null; then
    RY_CMD="python3 -c \"
from ruamel.yaml import YAML
yaml = YAML()
with open('$FIXTURE') as f:
    d = yaml.load(f)
keys = '$KEY'.split('.')
for k in keys:
    d = d[k]
print(d)
\""
    run_bench "python3+ruamel.yaml" "$RY_CMD"
else
    printf "  %-22s  (ruamel.yaml not installed — skip)\n" "python3+ruamel.yaml"
fi

echo ""
echo "Verify correctness:"
printf "  yamlget:     %s\n" "$($YAMLGET "$FIXTURE" "$KEY" 2>/dev/null || echo 'ERROR')"
if command -v yq > /dev/null 2>&1; then
    printf "  yq:          %s\n" "$(yq e ".$KEY" "$FIXTURE" 2>/dev/null || echo 'ERROR')"
fi
if python3 -c "import yaml" 2>/dev/null; then
    printf "  python+yaml: %s\n" "$(python3 -c "
import yaml
with open('$FIXTURE') as f:
    d = yaml.safe_load(f)
keys = '$KEY'.split('.')
for k in keys:
    d = d[k]
print(d)
" 2>/dev/null || echo 'ERROR')"
fi

echo ""
echo "Note: timings measure full process startup + parse + output per invocation."
echo "      Run multiple times for stable results; first run may differ (cold cache)."
