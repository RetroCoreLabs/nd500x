#!/bin/sh
#
# check_ndbus_isolation.sh - enforce the src/ndbus/ rule at build time
#
# SPDX-License-Identifier: MIT
# Copyright (c) 2025-2026 Ronny Hansen
#
# Nothing under src/ndbus/ may include an emulator type. It models the MFbus -
# the MPM-5 shared memory and the octobus - and reaches both machines only
# through the two vtables in ndbus_types.h.
#
# The rule is what keeps test layers 1, 3 and 4 runnable with two mock CPUs and
# no emulator linked, and it is what will make lifting this directory into its
# own repository a move rather than a rewrite. A rule nothing checks is a rule
# that is already broken, so this runs as part of the build.
#
# The repository root is derived from this script's own location: never a
# machine-specific path.

set -e

root=$(cd "$(dirname "$0")/.." && pwd)
dir="$root/src/ndbus"

if [ ! -d "$dir" ]; then
    echo "check_ndbus_isolation: $dir does not exist"
    exit 1
fi

# Headers that would drag an emulator type in. Matched against the text inside
# the #include quotes or angle brackets.
forbidden='machine_types\.h|machine\.h|machine_protos\.h|cpu_types\.h|cpu\.h|nd500_[a-z_]*\.h|debug_api\.h|ndlib\.h'

status=0
for file in "$dir"/*.c "$dir"/*.h; do
    [ -e "$file" ] || continue
    bad=$(grep -n '^[[:space:]]*#[[:space:]]*include' "$file" \
          | grep -E "[\"<]($forbidden)[\">]" || true)
    if [ -n "$bad" ]; then
        echo "check_ndbus_isolation: FORBIDDEN include in ${file#"$root"/}:"
        echo "$bad" | sed 's/^/    /'
        status=1
    fi
done

# A relative include that climbs out of src/ndbus/ reaches the emulator even
# when the basename looks harmless.
for file in "$dir"/*.c "$dir"/*.h; do
    [ -e "$file" ] || continue
    bad=$(grep -n '^[[:space:]]*#[[:space:]]*include[[:space:]]*"\.\./' "$file" || true)
    if [ -n "$bad" ]; then
        echo "check_ndbus_isolation: include climbs out of src/ndbus in ${file#"$root"/}:"
        echo "$bad" | sed 's/^/    /'
        status=1
    fi
done

if [ "$status" -eq 0 ]; then
    echo "check_ndbus_isolation: OK - src/ndbus/ includes no emulator type"
fi
exit $status
