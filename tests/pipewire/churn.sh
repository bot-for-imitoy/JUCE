#!/usr/bin/env bash
#
# Repeats the open-close test until it fails, which is the closest thing to a
# brute-force test that this harness has: one run of open-close opens and closes
# a device 50 times, so the default of 20 runs is 1000 open/close cycles.
#
# Every repetition is a fresh process. That matters: a leaked stream or a
# connection left behind by a previous device only shows up as a crash or a
# timeout in the *next* run, and the whole point of repeating it is to give
# those faults a chance to surface.
#
# The result of each run is appended to logs/churn.result, and the exit status
# is non-zero if anything went wrong, so this can be dropped into a CI job.
#
# Usage:
#   ./churn.sh              # 20 runs
#   ./churn.sh 100          # 100 runs
#
# Environment:
#   PIPEWIRE_TEST_BIN   path to the test binary (defaults to the build tree)
#   CHURN_TIMEOUT       per-run timeout in seconds (default 40)

set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bin="${PIPEWIRE_TEST_BIN:-$here/build/PipeWireStressTest_artefacts/RelWithDebInfo/PipeWireStressTest}"
n="${1:-20}"
timeoutSeconds="${CHURN_TIMEOUT:-40}"

if [ ! -x "$bin" ]; then
    echo "no test binary at $bin" >&2
    exit 1
fi

mkdir -p "$here/logs"
result="$here/logs/churn.result"
: > "$result"

crash=0; fail=0; hang=0; ok=0

for i in $(seq 1 "$n"); do
    log="$here/logs/churn-$i.log"

    timeout "$timeoutSeconds" "$bin" open-close > "$log" 2>&1
    rc=$?

    if [ $rc -eq 124 ]; then
        hang=$((hang + 1)); echo "run $i: HANG (no result after ${timeoutSeconds}s)" >> "$result"
    elif grep -q "signal 6\|signal 11" "$log"; then
        crash=$((crash + 1)); echo "run $i: CRASH" >> "$result"
    elif grep -q "FAILED" "$log"; then
        fail=$((fail + 1)); echo "run $i: $(grep -o 'FAILED: .*' "$log" | head -1)" >> "$result"
    else
        ok=$((ok + 1)); echo "run $i: ok" >> "$result"
    fi
done

echo "RESULT ok=$ok crash=$crash fail=$fail hang=$hang" | tee -a "$result"

[ $((crash + fail + hang)) -eq 0 ]
