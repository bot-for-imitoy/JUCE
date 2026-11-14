#!/usr/bin/env bash
#
# Destroys every PipeWire node with the given node.name. Used by the hotplug
# test to make a device disappear while it is in use.

set -uo pipefail

name="${1:?usage: pw-remove-node.sh <node.name>}"

ids="$(pw-dump 2>/dev/null | python3 -c '
import json, sys

name = sys.argv[1]
nodes = json.load (sys.stdin)
ids = [str (n["id"]) for n in nodes
       if ((n.get ("info") or {}).get ("props") or {}).get ("node.name") == name]
print (" ".join (ids))
' "$name")"

if [ -z "$ids" ]; then
    echo "no node named $name" >&2
    exit 1
fi

# shellcheck disable=SC2086
pw-cli destroy $ids
