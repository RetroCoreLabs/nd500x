#!/bin/bash
# Machine-specific roots come from the environment, never from the file.
#   NDIX      NDIX-C checkout (disk images + kernel/)
#   NDIX_B    NDIX-B checkout (authentic 1988 headers/sources)
#   PCC_ND500 pcc-nd500 checkout (the cross toolchain, its bin/)
# The nd500x repo root is derived from this script's own location.
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
: "${NDIX:?set NDIX to your NDIX-C checkout}"

# mkimage.sh <rootproto> <usrproto> <outimage>
# Build a complete di70 NDIX disk from two prototype files.
#   blocks     0 ..  8031  partition a  (root FFS, 7942 blocks at sector 90)
#   blocks  8100 .. 24819  partition b  (swap, zeros)
#   blocks 32850 .. 60817  partition e  (/usr FFS, 27968 blocks at +90)
# Geometry from baseline/etc/disktab "dio70" (ns18 nt5), verified in
# $NDIX/rootfs.build.md.
# The image is assembled on local disk and copied to the destination once -
# writing it block by block onto the Windows mount takes many minutes.
set -e
SP=/tmp/claude-1000/-home-ronny-repos-ragge-pcc-nd500/430a9137-7e6f-464c-b827-658eaac82a1c/scratchpad
B=${PCC_ND500:?set PCC_ND500}/bin
RPROTO=$1; UPROTO=$2; OUT=$3
W=$SP/imgbuild
mkdir -p $W
rm -f $W/nroot.img $W/nusr.img $W/full.img
$B/nd500-mkfs di0a  7942 18 5 8192 1024 16 10 60 2048 $W/nroot.img > $W/mkfs_root.log
$B/nd500-mkfs di0e 27968 18 5 8192 1024 16 10 60 2048 $W/nusr.img  > $W/mkfs_usr.log
$B/nd500-mkproto $W/nroot.img "$RPROTO"
$B/nd500-mkproto $W/nusr.img  "$UPROTO"
truncate -s 71147520 $W/full.img
dd if=$W/nroot.img of=$W/full.img bs=1M conv=notrunc status=none
# partition e starts at block 32760 of the image = byte 33546240
dd if=$W/nusr.img of=$W/full.img bs=1M seek=33546240 oflag=seek_bytes conv=notrunc status=none
cp $W/full.img "$OUT"
sync
ls -l "$OUT"
echo "MKIMAGE-DONE"
