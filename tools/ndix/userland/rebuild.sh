#!/bin/bash
# Regenerate the proto from the real image, rebuild the root partition, and
# splice it into a copy. Nothing writes to an image in use.
set -u
NDIX=/mnt/e/Dev/Ronny/NDIX-C
SP=/home/ronny/repos/nd500x/work
B=/home/ronny/repos/ragge/pcc-nd500/bin
SRC=$NDIX/rootfs_full.img
# The generator that sits beside this script. This used to point into a
# session scratch directory, so edits to the committed copy silently had no
# effect - the /etc overlay was generated and then ignored.
GEN=$(dirname "$0")/genproto2.py
OUT=$SP/stage/rootfs_new.img

cd /home/ronny/repos/nd500x || exit 1
rm -rf "$SP/stage/full"
mkdir -p "$SP/stage/full"

echo "=== 0. halt and newboot are the reboot binary under other names ==="
# shutdown(8) execs /etc/halt (shutdown.c:235). Without it the exec fails and
# "shutdown now" prints its warnings and then does nothing at all - which is
# exactly the symptom that was reported. There is no halt.c in the archive
# because 4.3 does not need one: reboot.c:59 takes progname from argv[0] and
# reboot.c:105 switches on its FIRST CHARACTER -
#     'h' -> halt    (howto = 0, the plain halt)
#     'r' -> reboot  (howto |= RB_SAME)
#     'n' -> newboot (boot a named kernel)
# so one binary installed under three names is how it was always shipped.
# genproto2.py installs every regular file in $SP/bin into /etc, so copying
# here is all that is needed - and doing it here rather than by hand means a
# later rebuild cannot silently drop it again.
for name in halt newboot; do
    cp "$SP/bin/reboot" "$SP/bin/$name" || exit 1
    printf '  %s\n' "$name"
done

echo "=== 1. proto from the real image ==="
# The 6th argument is the /etc overlay: the network database files the shipped
# image never had (services, protocols, networks, hosts - all from NDIX-C's own
# baseline/etc) and a replacement /etc/rc that brings lo0 up and starts inetd.
python3 "$GEN" "$SP/stage/listing.txt" "$SRC" "$SP/stage/full" \
        "$SP/stage/full.proto" "$SP/bin" "$SP/extraetc" 2>&1 | tail -4

echo
echo "=== 2. build the root filesystem ==="
W=$SP/imgbuild; mkdir -p "$W"; rm -f "$W/nroot.img"
"$B/nd500-mkfs" di0a 7942 18 5 8192 1024 16 10 60 2048 "$W/nroot.img" >"$W/mkfs.log" 2>&1
[ -s "$W/nroot.img" ] || { echo "  mkfs FAILED"; tail -3 "$W/mkfs.log"; exit 1; }
"$B/nd500-mkproto" "$W/nroot.img" "$SP/stage/full.proto" >"$W/mkproto.log" 2>&1
tail -2 "$W/mkproto.log" | sed 's/^/  /'

echo
echo "=== 3. splice the new root into a copy, leaving /usr alone ==="
cp "$SRC" "$OUT"
dd if="$W/nroot.img" of="$OUT" bs=1M conv=notrunc status=none
ls -l "$OUT" | sed 's/^/  /'
