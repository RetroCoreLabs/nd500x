#!/bin/bash
# Build ONE NDIX userland program from source: compile, then close the link
# against the libc we just rebuilt.
#
#   prog.sh <name> <src.c> [more.c ...]
#
# Uses tools/ndix/linkclose3.sh, which resolves undefined symbols iteratively
# against work/libx/symindex.txt - the ND-500 ld has no archive support, so the
# members have to be found and added by hand until the link closes.
set -u
export NDIX=/mnt/e/Dev/Ronny/NDIX-C
export NDIX_B=/mnt/e/Dev/Ronny/NDIX-B
export PCC_ND500=/home/ronny/repos/ragge/pcc-nd500
export ND500X_WORK=/home/ronny/repos/nd500x/work

B=$PCC_ND500/bin
SP=$ND500X_WORK
# CPPFLAGS lets a caller add a search path - conf and config need the directory
# holding their yacc-generated y.tab.h, which lives in the work tree.
INC="-I$SP/init/inc ${CPPFLAGS:-}"
LC=/home/ronny/repos/nd500x/tools/ndix/linkclose3.sh

OUT=$1; shift
W=$SP/usrbuild
mkdir -p "$W" "$SP/bin"
OBJS=""
: > "$W/$OUT.err"

for f in "$@"; do
    bn=$(basename "$f" .c)
    if ! $B/nd500-cpp $INC "$f" > "$W/$OUT.$bn.i" 2>>"$W/$OUT.err"; then
        echo "CPP FAIL $OUT ($bn)"; head -3 "$W/$OUT.err"; exit 1
    fi
    if ! $B/nd500-cc1 < "$W/$OUT.$bn.i" > "$W/$OUT.$bn.ic" 2>>"$W/$OUT.err"; then
        echo "CC1 FAIL $OUT ($bn)"; head -3 "$W/$OUT.err"; exit 1
    fi
    if ! $B/nd500-cc2 < "$W/$OUT.$bn.ic" > "$W/$OUT.$bn.s" 2>>"$W/$OUT.err"; then
        echo "CC2 FAIL $OUT ($bn)"; head -3 "$W/$OUT.err"; exit 1
    fi
    if ! $B/nd500-as "$W/$OUT.$bn.s" -o "$W/$OUT.$bn.o" 2>>"$W/$OUT.err"; then
        echo "AS FAIL $OUT ($bn)"; head -3 "$W/$OUT.err"; exit 1
    fi
    OBJS="$OBJS $W/$OUT.$bn.o"
done

if ! bash "$LC" "$W/$OUT" "$SP/libtest/crt0.o" $OBJS >>"$W/$OUT.err" 2>&1; then
    echo "LINK FAIL $OUT"; tail -4 "$W/$OUT.err"; exit 1
fi
if ! tail -1 "$W/$OUT.err" | grep -q "^CLOSED"; then
    echo "LINK NOT CLOSED $OUT"; tail -4 "$W/$OUT.err"; exit 1
fi

cp "$W/$OUT" "$SP/bin/$OUT"
MAG=$($B/nd500-dump -a "$SP/bin/$OUT" 2>/dev/null | grep -io "0x010b\|0x0109\|0x0107" | head -1)
echo "OK $OUT  magic=$MAG  size=$(stat -c%s "$SP/bin/$OUT")"
