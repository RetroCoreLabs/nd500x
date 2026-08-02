#!/bin/bash
# Machine-specific roots come from the environment, never from the file.
#   NDIX      NDIX-C checkout (disk images + kernel/)
#   NDIX_B    NDIX-B checkout (authentic 1988 headers/sources)
#   PCC_ND500 pcc-nd500 checkout (the cross toolchain, its bin/)
# The nd500x repo root is derived from this script's own location.
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
: "${NDIX:?set NDIX to your NDIX-C checkout}"

# Scratch/work area. Override with ND500X_WORK; defaults inside the repo.
S="${ND500X_WORK:-$REPO_ROOT/work}"
mkdir -p "$S"
cd "$REPO_ROOT"
hits=0
for i in 1 2; do
  P=$((5180+i))
  rm -f $S/a$i.log
  ( sleep 560 ) | timeout 580 ./build/bin/nd500x \
      --ndix $NDIX/rootfs_full.img \
      --kernel $S/autoboot/vmunix --telnet=$P > $S/a$i.log 2>&1 &
  EPID=$!
  for w in $(seq 1 60); do grep -q "login:" $S/a$i.log 2>/dev/null && break; sleep 5; done
  if grep -q "login:" $S/a$i.log 2>/dev/null; then
    timeout 220 python3 $S/exp4.py $P > /dev/null 2>&1
    sleep 5
    c=$(grep -c 4700204D $S/a$i.log)
    [ "$c" != "0" ] && hits=$((hits+1))
    echo "auto run $i: wild=$c uarea=$(grep -c 'u-area published' $S/a$i.log) shell=$(grep -c 'Welcome back to 1988' $S/a$i.log)"
  else
    echo "auto run $i: NO BOOT"
  fi
  kill $EPID 2>/dev/null; wait $EPID 2>/dev/null
done
echo "AUTO_HITS=$hits/2"
