#!/bin/bash
# List what is ACTUALLY in the image's root partition.
#
# stage24.proto turned out to describe an older filesystem than the one in use -
# rebuilding from it silently dropped fsck, mount, dump, cron, syslogd and the
# whole of /lib. So the prototype has to be built from the image itself.
#
# "ls -l" gives everything a proto line needs: type, mode, and for a device the
# major and minor. /usr is a separate partition and is not touched.
set -u
cd /home/ronny/repos/nd500x || exit 1
IMG=${1:-/mnt/e/Dev/Ronny/NDIX-C/rootfs_full.img}
OUT=${2:-/home/ronny/repos/nd500x/work/stage/listing.txt}
mkdir -p "$(dirname "$OUT")"
cp "$IMG" /tmp/en.img

{
    sleep 50
    printf 'root\n'; sleep 6
    for d in / /etc /bin /dev /lib /tmp; do
        printf '#### %s\n' "$d"
        printf 'ls -l %s\n' "$d"
        sleep 8
    done
    printf '~ndix-halt\n'
    sleep 30
} | timeout 300 ./build/bin/nd500x --ndix /tmp/en.img 2>&1 \
  | sed -n '/Welcome/,$ p' | grep -avE '^\[FECALL|^\[ndix\]' > "$OUT"

echo "listing written: $OUT  ($(wc -l < "$OUT") lines)"
echo
echo "=== top-level entries found ==="
sed -n '/#### \/$/,/#### \/etc/p' "$OUT" | head -20
