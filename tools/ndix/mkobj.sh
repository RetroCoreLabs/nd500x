#!/bin/bash
# Machine-specific roots come from the environment, never from the file.
#   NDIX      NDIX-C checkout (disk images + kernel/)
#   NDIX_B    NDIX-B checkout (authentic 1988 headers/sources)
#   PCC_ND500 pcc-nd500 checkout (the cross toolchain, its bin/)
# The nd500x repo root is derived from this script's own location.
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
: "${NDIX:?set NDIX to your NDIX-C checkout}"

# mkobj.sh <out.o> <src.c>  - compile ONE file with $CPPFLAGS to a named object.
SP=/tmp/claude-1000/-home-ronny-repos-ragge-pcc-nd500/430a9137-7e6f-464c-b827-658eaac82a1c/scratchpad
B=${PCC_ND500:?set PCC_ND500}/bin
O=$1; SRC=$2
W=$(dirname "$O"); mkdir -p "$W"
bn=$(basename "$O" .o)
ERR=$W/$bn.err
$B/nd500-cpp -I$SP/init/inc -I${NDIX_B:?set NDIX_B}/usr.include $CPPFLAGS "$SRC" > $W/$bn.i 2>$ERR || { echo "CPP FAIL $SRC"; tail -5 $ERR; exit 1; }
$B/nd500-cc1 < $W/$bn.i > $W/$bn.ic 2>>$ERR || { echo "CC1 FAIL $SRC"; tail -15 $ERR; exit 1; }
$B/nd500-cc2 < $W/$bn.ic > $W/$bn.s 2>>$ERR || { echo "CC2 FAIL $SRC"; tail -15 $ERR; exit 1; }
$B/nd500-as $W/$bn.s -o $O 2>>$ERR || { echo "AS FAIL $SRC"; tail -15 $ERR; exit 1; }
echo "obj OK $O"
