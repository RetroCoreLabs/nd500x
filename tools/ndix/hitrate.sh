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
for i in $(seq 1 6); do
  ( sleep 105 ) | ND500X_STOPDBG=1 timeout 120 ./build/bin/nd500x \
      --ndix $NDIX/rootfs_full.img \
      --kernel $NDIX/kernel/MASTER/GENERIC/vmunix \
      > $S/boot_run_$i.log 2>&1
  if grep -q "4700204D" $S/boot_run_$i.log; then
    hits=$((hits+1)); echo "boot $i: HIT"
  else
    echo "boot $i: clean"
  fi
done
echo "TOTAL_HITS=$hits/6"
