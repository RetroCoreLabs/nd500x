#!/bin/bash
# Machine-specific roots come from the environment, never from the file.
#   NDIX      NDIX-C checkout (disk images + kernel/)
#   NDIX_B    NDIX-B checkout (authentic 1988 headers/sources)
#   PCC_ND500 pcc-nd500 checkout (the cross toolchain, its bin/)
# The nd500x repo root is derived from this script's own location.
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
: "${NDIX:?set NDIX to your NDIX-C checkout}"

S=/tmp/claude-1000/-home-ronny-repos-ragge-pcc-nd500/430a9137-7e6f-464c-b827-658eaac82a1c/scratchpad
cd "$REPO_ROOT"
for i in 1 2 3; do
  P=$((5110+i))
  ( sleep 300 ) | ND500X_ZERO_PST="16,21,26,31,36,41" ND500X_UAREADBG=1 timeout 320 \
    ./build/bin/nd500x --ndix $NDIX/rootfs_full.img \
    --kernel $NDIX/kernel/MASTER/GENERIC/vmunix --telnet=$P > $S/z$i.log 2>&1 &
  EPID=$!
  until grep -q "login:" $S/z$i.log 2>/dev/null; do sleep 5; done
  timeout 200 python3 $S/exp4.py $P > /dev/null 2>&1
  sleep 5
  echo "run $i: wild=$(grep -c 4700204D $S/z$i.log) BAD=$(grep -c BAD $S/z$i.log) zerofault=$(grep -c 'is ZERO' $S/z$i.log)"
  wait $EPID 2>/dev/null
done
echo DONE
