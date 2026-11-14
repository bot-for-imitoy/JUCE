#!/usr/bin/env bash
#
# Runs the PipeWire stress tests against the PipeWire session that is already
# running on this machine.
#
# A couple of the tests need a device that can be unplugged and a microphone
# that produces a known signal, so this script adds two temporary virtual nodes
# to the running graph for the duration of the run and removes them again
# afterwards. Nothing else in the session is touched: the real sinks and sources
# are left alone, and the tests that would disturb them (stopping the server)
# stay skipped unless they are asked for explicitly.
#
# Usage:
#   ./run-tests.sh              # all tests
#   ./run-tests.sh hotplug      # one test
#   ./run-tests.sh --list       # list the tests
#
# Environment:
#   PIPEWIRE_TEST_BIN   path to the test binary (defaults to the build tree)
#   KEEP_TEST_NODES=1   leave the temporary nodes in the graph afterwards
#
# The restart-server test kills the PipeWire server and starts it again, so it
# takes the whole session's audio down for a moment and stays skipped unless it
# is asked for explicitly:
#
#   JUCE_PW_DAEMON_PID=$(systemctl --user show pipewire -p MainPID --value) \
#   JUCE_PW_DAEMON_CMD='systemctl --user reset-failed pipewire.service; systemctl --user restart pipewire' \
#   ./run-tests.sh restart-server
#
# It has to run where the user bus is reachable, because that is what restarts
# the daemon: inside a container or a sandboxed shell the PID is invisible and
# the restart command fails. Starting the daemon by hand instead of going
# through systemd leaves a second server running, so prefer the command above.
#
# The reset-failed is not optional when the test is repeated: every run kills
# the daemon, and systemd rate-limits restarts (DefaultStartLimitBurst, 5 in 10s
# on most distributions). Once that limit is hit the unit stays in
# "start-limit-hit" and no restart happens at all, which the test then reports
# as "could not restart the server" - with the server gone, so the whole session
# has no audio until it is started again by hand. Leave a few seconds between
# runs as well.

set -uo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
bin="${PIPEWIRE_TEST_BIN:-$here/build/PipeWireStressTest_artefacts/RelWithDebInfo/PipeWireStressTest}"

sink_name="juce-stress-sink"
source_name="juce-stress-source"
hotplug_name="juce-hotplug-sink"

if [ ! -x "$bin" ]; then
    echo "no test binary at $bin" >&2
    echo "build it first:" >&2
    echo "  cmake -S '$here' -B '$here/build' -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo" >&2
    echo "  cmake --build '$here/build'" >&2
    exit 1
fi

if ! pw-cli info >/dev/null 2>&1; then
    echo "cannot talk to a PipeWire session; is it running?" >&2
    exit 1
fi

cleanup() {
    if [ "${KEEP_TEST_NODES:-0}" = "1" ]; then
        echo "temporary nodes left in the graph: $sink_name, $source_name"
        return
    fi

    for name in "$sink_name" "$source_name" "$hotplug_name"; do
        "$here/pw-remove-node.sh" "$name" >/dev/null 2>&1 || true
    done
}

trap cleanup EXIT

# Remove leftovers from an earlier interrupted run before adding ours.
cleanup >/dev/null 2>&1

pw-cli create-node adapter "{
    factory.name = support.null-audio-sink
    node.name = $sink_name
    node.description = \"JUCE stress sink\"
    media.class = Audio/Sink
    object.linger = true
    audio.position = [ FL FR ]
}" >/dev/null || { echo "could not create $sink_name" >&2; exit 1; }

pw-cli create-node adapter "{
    factory.name = support.null-audio-sink
    node.name = $source_name
    node.description = \"JUCE test source\"
    media.class = Audio/Source
    object.linger = true
    audio.position = [ FL FR ]
    monitor.channel-volumes = true
}" >/dev/null || { echo "could not create $source_name" >&2; exit 1; }

# Give the graph a moment to publish the new nodes.
sleep 1

export JUCE_PW_ALLOW_GRAPH_CHANGES=1
export JUCE_PW_OUTPUT_NAME="JUCE stress sink"
export JUCE_PW_INPUT_NAME="JUCE test source"

export JUCE_PW_HOTPLUG_CMD="pw-cli create-node adapter '{ factory.name=support.null-audio-sink node.name=$hotplug_name node.description=\"JUCE hotplug sink\" media.class=Audio/Sink object.linger=true audio.position=[ FL FR ] }'"
export JUCE_PW_HOTPLUG_REMOVE_CMD="$here/pw-remove-node.sh $hotplug_name"

"$bin" "$@"
