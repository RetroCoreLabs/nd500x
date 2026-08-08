#!/bin/sh
#
# What can this checkout actually reach?
#
# Most of the material this emulator is debugged against - the NDIX trees, the
# vendor DOM and NRF files, the cross toolchain, the carved SINTRAN corpus -
# is too large to vendor and lives outside the repository. It is located
# through environment variables, listed in docs/PATH_CONVENTIONS.md.
#
# Unset or wrong, those variables do not announce themselves. A script fails
# with "file not found" on a path nobody chose, a CMake test silently skips,
# or a search goes off hunting for a tree that was never checked out. This
# prints the answer once, up front.
#
# For each variable: is it set, does it resolve to something that exists, and
# does that something look like the tree it claims to be? The third question is
# the one that matters - a directory called "ndix" holding a disk image and a
# few .bat files is a place to RUN NDIX from, not the NDIX-C source checkout,
# and only a marker check tells the two apart.
#
# Run it as `make doctor`, or directly. It reads only; nothing is created or
# modified.
#
# Exit status:
#   0  nothing is misconfigured (variables may be unset - that is not an error)
#   1  at least one variable is set but points somewhere wrong

REPO_ROOT=$(cd "$(dirname "$0")/.." && pwd)

# VAR | kind | marker relative to the tree | what the tree is
#
# kind is "dir" or "file", describing the marker, not the variable.
# Markers are subpaths this repository actually references, so a tree that
# lacks one genuinely cannot serve the use it is named for.
# An empty marker means existence is all that can be checked.
#
# Fed to the loop below by here-document rather than a pipe: the right-hand
# side of a pipe is a subshell, and the tallies have to survive the loop.
SPEC='
NDIX|dir|kernel/MASTER|NDIX-C checkout (disk images, kernel/MASTER/, notes/)
NDIX_B|dir|usr.include|NDIX-B checkout (authentic 1988 headers and sources)
PCC_ND500|dir|bin|pcc-nd500 cross toolchain
RETROCORE|dir|Emulated.HW/ND/CPU/ND500|RetroCore checkout (the C# ND-500 CPU)
ND500_TESTDATA|dir|FraTor|ND-500 test-material tree (.dom, .nrf, disk images)
ND500USERS|dir||SINTRAN user-area tree (GUEST/, SYSTEM/, SCRATCH/)
NDINSIGHT|dir|tools/sintran-segment-carver|NDInsight tree (the MON call corpus)
ND5000UC|dir|microcode|ND-5000 microcode and manual transcriptions
ND100X|dir||nd100x checkout
NDMONLIB|dir|include/ndmon|ndmonlib checkout, when used outside external/
RETROTERM|dir|src|RetroTerm checkout
LIBDAP|dir||libdap / mcp-dap-server checkout
ND500_FS|dir||SINTRAN filesystem root used by the JSON machine config
ND500X_WORK|dir||scratch area (defaults to work/ in the repo, gitignored)
ND500X_TAPE|file||SIMH .tap image for the tape device (generic 2)
ND500X_DISK|file||root disk image
'

set_ok=0
unset_count=0
bad=0

printf '%s\n' "Repository: $REPO_ROOT"
printf '%s\n' "Reference: docs/PATH_CONVENTIONS.md"
printf '\n'

printf '%-16s %-9s %s\n' "VARIABLE" "STATUS" "VALUE / WHAT IT IS FOR"
printf '%-16s %-9s %s\n' "----------------" "---------" "----------------------------------------"

while IFS='|' read -r var kind marker what; do
    [ -z "$var" ] && continue

    # No `${!var}` - this stays POSIX sh so it runs anywhere the build does.
    value=$(eval "printf '%s' \"\${$var-}\"")

    if [ -z "$value" ]; then
        printf '%-16s %-9s %s\n' "$var" "unset" "$what"
        unset_count=$((unset_count + 1))
        continue
    fi

    if [ "$kind" = file ]; then
        if [ ! -f "$value" ]; then
            printf '%-16s %-9s %s\n' "$var" "MISSING" "$value"
            printf '%-16s %-9s %s\n' "" "" "  no such file"
            bad=$((bad + 1))
            continue
        fi
    else
        if [ ! -d "$value" ]; then
            printf '%-16s %-9s %s\n' "$var" "MISSING" "$value"
            printf '%-16s %-9s %s\n' "" "" "  no such directory"
            bad=$((bad + 1))
            continue
        fi
        if [ -n "$marker" ] && [ ! -e "$value/$marker" ]; then
            printf '%-16s %-9s %s\n' "$var" "WRONG" "$value"
            printf '%-16s %-9s %s\n' "" "" "  exists, but contains no $marker"
            printf '%-16s %-9s %s\n' "" "" "  so it is not the $what"
            bad=$((bad + 1))
            continue
        fi
    fi

    printf '%-16s %-9s %s\n' "$var" "ok" "$value"
    set_ok=$((set_ok + 1))
done <<SPEC_END
$SPEC
SPEC_END

printf '\n'
printf '%s\n' "$set_ok usable, $unset_count unset, $bad set but wrong."

if [ "$bad" -gt 0 ]; then
    printf '\n'
    printf '%s\n' "A variable that is set but wrong is worse than an unset one: scripts"
    printf '%s\n' "take the configured path and fail later, somewhere else, as something"
    printf '%s\n' "that looks like a different problem."
    exit 1
fi

exit 0
