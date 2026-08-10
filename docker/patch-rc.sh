#!/bin/sh
#
# patch-rc.sh - put the guest's network configuration INTO the disk image, so
#               NDIX configures its own et0 at boot.
#
#   patch-rc.sh <writable-image> <etconfig-line> <ifconfig-line>
#
# WHY THIS IS BETTER THAN TYPING AT THE CONSOLE
# ---------------------------------------------
# The alternative - wait for `login:`, log in as root, type two commands and
# wait for each prompt - works, and this container can still fall back to it.
# But it is a timing dance with a 1988 machine over a console that drops the odd
# character, and every step is a chance to be unlucky. Patching /etc/rc removes
# the dance entirely: the guest brings its own interface up as part of its
# normal boot, exactly as a real machine would have done in 1988.
#
# HOW IT WORKS, AND THE TWO TRAPS
# -------------------------------
# tools/ndix/ndixput.py writes a file back into an NDIX filesystem in place. It
# only ever overwrites a file that ALREADY EXISTS, and only when the new
# contents keep the identical block footprint - FFS derives the last block's
# fragment count from di_size, so a size change across a fragment boundary would
# leave the free maps disagreeing with the inode. In practice that is a window
# of at most 1024 bytes, and --pad fills up to the top of it.
#
#   TRAP 1: /etc/rc ends with `exit 0`. Lines appended AFTER it are on disk,
#           are perfectly correct, and NEVER RUN. They must go BEFORE it.
#   TRAP 2: etconfig must come before ifconfig, and -trailers is mandatory on
#           the ifconfig line. Both are the caller's responsibility; this script
#           inserts whatever it is given, in the order it is given.
#
# /etc/rc in the shipped image is 1156 bytes in 2048 allocated, so there is
# about 892 bytes of headroom - far more than the two lines need.
#
set -eu

IMG="$1"
ETLINE="$2"
IFLINE="$3"

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

RC="$WORK/rc"
python3 /opt/nd500x/bin/ndixput.py "$IMG" /etc/rc "$RC" --get

# Refuse to stack our block on top of a previous one. The image may already have
# been patched (a re-run, or a user's own image), and appending a second copy
# would waste the headroom and eventually not fit.
if grep -q 'ndix-docker: network' "$RC"; then
    echo "patch-rc: /etc/rc already carries a network block, removing the old one" >&2
    sed -i '/# ndix-docker: network - BEGIN/,/# ndix-docker: network - END/d' "$RC"
fi

# Insert before the LAST `exit 0`. Anything after it never runs.
if ! grep -q '^[[:space:]]*exit 0' "$RC"; then
    echo "patch-rc: no 'exit 0' found in /etc/rc - refusing to guess where to insert" >&2
    exit 1
fi

NEW="$WORK/rc.new"
awk -v et="$ETLINE" -v ifc="$IFLINE" '
    /^[[:space:]]*exit 0/ && !done {
        print "# ndix-docker: network - BEGIN"
        # EVERY line in this file redirects to /dev/console, and so must ours.
        # rc does not run with the console as its stdout, so a bare echo (or an
        # error message from etconfig) goes nowhere and the block looks like it
        # never ran at all. That cost an hour once already.
        print "/bin/echo \"ndix-docker: configuring et0\" > /dev/console 2>&1"
        # ORDER IS NOT OPTIONAL: etinit() returns immediately unless the
        # interface has both an address and ET_SET, which etconfig sets.
        print et " > /dev/console 2>&1"
        print ifc " > /dev/console 2>&1"
        print "/etc/ifconfig et0 > /dev/console 2>&1"
        print "# ndix-docker: network - END"
        done = 1
    }
    { print }
' "$RC" > "$NEW"

# --pad tops the file up to the largest size that keeps the same block
# footprint, so anything that fits at all is accepted rather than rejected for
# landing a few bytes short of a fragment boundary.
python3 /opt/nd500x/bin/ndixput.py "$IMG" /etc/rc "$NEW" --pad '\n'

echo "patch-rc: /etc/rc patched - the guest will configure et0 itself at boot" >&2
