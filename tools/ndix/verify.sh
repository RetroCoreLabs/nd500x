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
hits=0
for i in 1 2 3; do
  P=$((5160+i))
  rm -f $S/v$i.log
  ( sleep 560 ) | timeout 580 ./build/bin/nd500x \
      --ndix $NDIX/rootfs_full.img \
      --kernel $NDIX/kernel/MASTER/GENERIC/vmunix \
      --telnet=$P > $S/v$i.log 2>&1 &
  EPID=$!
  for w in $(seq 1 60); do grep -q "login:" $S/v$i.log 2>/dev/null && break; sleep 5; done
  if grep -q "login:" $S/v$i.log 2>/dev/null; then
    timeout 220 python3 $S/exp4.py $P > /dev/null 2>&1
    sleep 5
    c=$(grep -c 4700204D $S/v$i.log)
    [ "$c" != "0" ] && hits=$((hits+1))
    echo "run $i: wild=$c uarea=$(grep -c 'u-area published' $S/v$i.log)"
  else
    echo "run $i: NO BOOT"
  fi
  kill $EPID 2>/dev/null; wait $EPID 2>/dev/null
done
echo "VERIFY_HITS=$hits/3 (baseline was ~4 in 6)"
