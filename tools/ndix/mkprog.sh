#!/bin/bash
# Machine-specific roots come from the environment, never from the file.
#   NDIX      NDIX-C checkout (disk images + kernel/)
#   NDIX_B    NDIX-B checkout (authentic 1988 headers/sources)
#   PCC_ND500 pcc-nd500 checkout (the cross toolchain, its bin/)
# The nd500x repo root is derived from this script's own location.
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
: "${NDIX:?set NDIX to your NDIX-C checkout}"

# mkprog.sh <outname> <src.c ...>  - compile+link a multi-file NDIX program.
# Env: CPPFLAGS extra -I/-D flags; OUTDIR (default $SP/nat/bin); WDIR work area.
# Scratch/work area. Override with ND500X_WORK; defaults inside the repo.
SP="${ND500X_WORK:-$REPO_ROOT/work}"
mkdir -p "$SP"
B=${PCC_ND500:?set PCC_ND500}/bin
OUT=$1; shift
OUTDIR=${OUTDIR:-$SP/nat/bin}
W=${WDIR:-$SP/nat/obj/$OUT}
mkdir -p "$OUTDIR" "$W"
ERR=$W/build.err
: > $ERR
OBJS=""
for f in "$@"; do
  case "$f" in *.o) OBJS="$OBJS $f"; continue;; esac
  bn=$(basename "$f" .c)
  # keep unique object names when the same basename appears twice
  o=$W/$bn.o
  n=1
  while echo " $OBJS " | grep -q " $o "; do o=$W/$bn$n.o; n=$((n+1)); done
  echo "--- $f" >> $ERR
  $B/nd500-cpp -I$SP/init/inc -I${NDIX_B:?set NDIX_B}/usr.include $CPPFLAGS "$f" > $W/$bn.i 2>>$ERR || { echo "CPP FAIL $f"; tail -5 $ERR; exit 1; }
  $B/nd500-cc1 < $W/$bn.i > $W/$bn.ic 2>>$ERR || { echo "CC1 FAIL $f"; tail -15 $ERR; exit 1; }
  $B/nd500-cc2 < $W/$bn.ic > $W/$bn.s 2>>$ERR || { echo "CC2 FAIL $f"; tail -15 $ERR; exit 1; }
  $B/nd500-as $W/$bn.s -o $o 2>>$ERR || { echo "AS FAIL $f"; tail -15 $ERR; exit 1; }
  OBJS="$OBJS $o"
done
$SP/linkclose3.sh $OUTDIR/$OUT $SP/libtest/crt0.o $OBJS >>$ERR 2>&1
tail -1 $ERR | grep -q "^CLOSED" || { echo "LINK FAIL $OUT"; tail -8 $ERR; exit 1; }
MAG=$($B/nd500-dump -a $OUTDIR/$OUT 2>/dev/null | grep -io "0x010b\|0x0109" | head -1)
SZ=$(stat -c %s $OUTDIR/$OUT)
echo "OK $OUT magic=$MAG size=$SZ"
[ "$MAG" = "0x010b" ] || exit 2
