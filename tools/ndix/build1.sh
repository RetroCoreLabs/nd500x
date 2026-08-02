#!/bin/bash
# Machine-specific roots come from the environment, never from the file.
#   NDIX      NDIX-C checkout (disk images + kernel/)
#   NDIX_B    NDIX-B checkout (authentic 1988 headers/sources)
#   PCC_ND500 pcc-nd500 checkout (the cross toolchain, its bin/)
# The nd500x repo root is derived from this script's own location.
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
: "${NDIX:?set NDIX to your NDIX-C checkout}"

# build1.sh <src.c> <outname> [extra .c ...] - compile+link one NDIX program
# Scratch/work area. Override with ND500X_WORK; defaults inside the repo.
SP="${ND500X_WORK:-$REPO_ROOT/work}"
mkdir -p "$SP"
B=${PCC_ND500:?set PCC_ND500}/bin
SRC=$1; OUT=$2; shift 2
W=$SP/usrbuild
CPPFLAGS=${CPPFLAGS:-}
OBJS=""
for f in "$SRC" "$@"; do
  bn=$(basename "$f" .c)
  $B/nd500-cpp -I$SP/init/inc -I${NDIX_B:?set NDIX_B}/usr.include $CPPFLAGS "$f" > $W/$OUT.$bn.i 2>$W/$OUT.err || { echo "CPP FAIL"; exit 1; }
  $B/nd500-cc1 < $W/$OUT.$bn.i > $W/$OUT.$bn.ic 2>>$W/$OUT.err || { echo "CC1 FAIL"; exit 1; }
  $B/nd500-cc2 < $W/$OUT.$bn.ic > $W/$OUT.$bn.s 2>>$W/$OUT.err || { echo "CC2 FAIL"; exit 1; }
  $B/nd500-as $W/$OUT.$bn.s -o $W/$OUT.$bn.o 2>>$W/$OUT.err || { echo "AS FAIL"; exit 1; }
  OBJS="$OBJS $W/$OUT.$bn.o"
done
$SP/linkclose3.sh $W/$OUT $SP/libtest/crt0.o $OBJS >>$W/$OUT.err 2>&1
tail -1 $W/$OUT.err | grep -q "^CLOSED" || { echo "LINK FAIL"; tail -3 $W/$OUT.err; exit 1; }
MAG=$($B/nd500-dump -a $W/$OUT 2>/dev/null | grep -io "0x010b\|0x0109" | head -1)
echo "OK $OUT magic=$MAG"
