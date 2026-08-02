#!/bin/bash
# Machine-specific roots come from the environment, never from the file.
#   NDIX      NDIX-C checkout (disk images + kernel/)
#   NDIX_B    NDIX-B checkout (authentic 1988 headers/sources)
#   PCC_ND500 pcc-nd500 checkout (the cross toolchain, its bin/)
# The nd500x repo root is derived from this script's own location.
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
: "${NDIX:?set NDIX to your NDIX-C checkout}"

# linkclose2.sh <output> <object...> - like linkclose.sh but never resolves a
# symbol from libc when one of the PROGRAM's own objects already provides it
# as a COMMON (tentative definition, UNDF|EXT with nonzero value in the .o).
# Bournegol sh declares its globals in headers (INT flags; STRING tmpnam;...)
# so every object carries them as commons; ld -d allocates them at the end,
# but mid-closure they list as undefined and _tmpnam matched a libc member,
# producing a multiply-defined clash.
# Scratch/work area. Override with ND500X_WORK; defaults inside the repo.
SP="${ND500X_WORK:-$REPO_ROOT/work}"
mkdir -p "$SP"
B=${PCC_ND500:?set PCC_ND500}/bin
OUT=$1; shift
PROGOBJS="$*"
OBJS="$PROGOBJS"
ORDER="syslib genlib stdiolib netlib inetlib hostlib libm compat-4.1lib compat-sys5lib"
# Commons provided by the program's own objects: UNDF|EXT with value != 0
COMMONS=$(for o in $PROGOBJS; do $B/nd500-dump -s "$o" 2>/dev/null; done \
  | awk '/UNDF\|EXT/ && strtonum($4) != 0 {print $2}' | sort -u)
for iter in $(seq 1 40); do
  $B/nd500-ld -e start -x -d -o "$OUT" $OBJS 2>/dev/null
  UND=$($B/nd500-dump -u "$OUT" 2>/dev/null | awk '/UNDF\|EXT/ {print $2}' \
        | grep -av '^_etext$\|^_edata$\|^_end$')
  # drop symbols our own objects define as commons
  UND=$(comm -23 <(echo "$UND" | sort -u) <(echo "$COMMONS"))
  if [ -z "$UND" ]; then echo "CLOSED after $iter iterations"; exit 0; fi
  ADDED=0
  for sym in $UND; do
    member=""
    for lib in $ORDER; do
      m=$(awk -v s="$sym" -v l="$lib/" '$1==s && index($2,l)==1 {print $2; exit}' $SP/libx/symindex.txt)
      if [ -n "$m" ]; then member=$m; break; fi
    done
    if [ -n "$member" ] && ! echo " $OBJS " | grep -q " $SP/libx/$member "; then
      OBJS="$OBJS $SP/libx/$member"; ADDED=1
    fi
  done
  if [ $ADDED -eq 0 ]; then echo "STUCK; remaining undefined:"; echo "$UND"; exit 1; fi
done
echo "no convergence"; exit 1
