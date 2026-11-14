#!/usr/bin/env bash
#
# Repeats the restart-server test, which is the only test that takes the whole
# session's audio down: it kills the daemon and checks that the device rebuilds
# itself on its own. Running it a few times is what shows whether the recovery
# is reliable or just lucky, and it needs a real session, so unlike the other
# tests it has to run on the host rather than in a container.
#
# The pause between the runs is deliberate. Every run starts the daemon twice
# (systemd starts it after the kill, and then the test's restart command starts
# it again), and systemd rate-limits that: without the reset-failed and the gap,
# the fifth start attempt in ten seconds is refused, the unit stays in
# "start-limit-hit" and the machine is left without a server - not just without
# a test result.
#
# Usage:
#   ./restart-repeat.sh        # 3 runs
#   ./restart-repeat.sh 10
#
# Environment:
#   PIPEWIRE_TEST_BIN   path to the test binary (defaults to the build tree)

set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bin="${PIPEWIRE_TEST_BIN:-$here/build/PipeWireStressTest_artefacts/RelWithDebInfo/PipeWireStressTest}"
n="${1:-3}"

if [ ! -x "$bin" ]; then
    echo "no test binary at $bin" >&2
    exit 1
fi

mkdir -p "$here/logs"
result="$here/logs/restart-repeat.result"
: > "$result"

for i in $(seq 1 "$n"); do
    systemctl --user reset-failed pipewire.service >/dev/null 2>&1
    pid="$(systemctl --user show pipewire -p MainPID --value)"

    out="$(JUCE_PW_DAEMON_PID="$pid" \
           JUCE_PW_DAEMON_CMD="systemctl --user reset-failed pipewire.service; systemctl --user restart pipewire" \
           timeout 150 "$bin" restart-server 2>&1)"
    echo "$out" > "$here/logs/restart-$i.log"

    if echo "$out" | grep -q "PASS"; then
        echo "run $i: ok ($(echo "$out" | grep -o 'after the restart: [0-9]* new callbacks[^,]*'))" \
            >> "$result"
    else
        echo "run $i: $(echo "$out" | grep -o 'FAILED: .*' | head -1)" >> "$result"
    fi

    sleep 8
done

cat "$result"
! grep -q "FAILED" "$result"
