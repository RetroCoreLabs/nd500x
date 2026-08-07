#!/bin/bash
# Prove the rebuilt image LOST NOTHING and gained what it should.
#
# The previous attempt booted and looked fine, but was missing fsck, mount,
# dump and the whole of /lib - because it came from a stale prototype. Booting
# is not enough of a check: the file lists have to be compared both ways.
set -u
cd /home/ronny/repos/nd500x || exit 1
NEW=/home/ronny/repos/nd500x/work/stage/rootfs_new.img
OLD=/mnt/e/Dev/Ronny/NDIX-C/rootfs_full.img
W=/home/ronny/repos/nd500x/work/stage

listing() {   # listing <image> <outfile>
    cp "$1" /tmp/lst.img
    {
        sleep 50
        printf 'root\n'; sleep 6
        for d in / /etc /bin /dev /lib; do
            printf 'ls %s\n' "$d"; sleep 7
        done
        printf '~ndix-halt\n'; sleep 30
    } | timeout 300 ./build/bin/nd500x --ndix /tmp/lst.img 2>&1 \
      | sed -n '/Welcome/,$ p' | grep -avE '^\[|^#|^ls |^~|^sync|^syncing' \
      | tr ' ' '\n' | grep -av '^$' | sort -u > "$2"
}

echo "=== listing the OLD image ==="
listing "$OLD" "$W/old.list"
echo "  $(wc -l < "$W/old.list") names"

echo "=== listing the NEW image ==="
listing "$NEW" "$W/new.list"
echo "  $(wc -l < "$W/new.list") names"

echo
echo "=== LOST (in old, missing from new) - must be empty ==="
comm -23 "$W/old.list" "$W/new.list" | tr '\n' ' '
echo
echo
echo "=== GAINED (new programs) ==="
comm -13 "$W/old.list" "$W/new.list" | tr '\n' ' '
echo
