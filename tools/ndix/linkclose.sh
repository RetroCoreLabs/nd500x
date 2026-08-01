#!/bin/bash
# Machine-specific roots come from the environment, never from the file.
#   NDIX      NDIX-C checkout (disk images + kernel/)
#   NDIX_B    NDIX-B checkout (authentic 1988 headers/sources)
#   PCC_ND500 pcc-nd500 checkout (the cross toolchain, its bin/)
# The nd500x repo root is derived from this script's own location.
REPO_ROOT=$(cd "$(dirname "$0")/../.." && pwd)
: "${NDIX:?set NDIX to your NDIX-C checkout}"

# linkclose.sh <output> <object...>  - iteratively link, resolving undefined
# externals from the extracted 1988 libc member index (symindex.txt).
# Preference order when a symbol has multiple definers: syslib, gen, stdio,
# compat-4.1, compat-sys5 (first match in that order).
SP=/tmp/claude-1000/-home-ronny-repos-ragge-pcc-nd500/430a9137-7e6f-464c-b827-658eaac82a1c/scratchpad
B=${PCC_ND500:?set PCC_ND500}/bin
OUT=$1; shift
OBJS="$*"
ORDER="syslib genlib stdiolib netlib inetlib compat-4.1lib compat-sys5lib"
for iter in $(seq 1 40); do
  $B/nd500-ld -e start -x -d -o "$OUT" $OBJS 2>/dev/null
  UND=$($B/nd500-dump -u "$OUT" 2>/dev/null | awk '/UNDF\|EXT/ {print $2}' | grep -av '^_etext$\|^_edata$\|^_end$')
  if [ -z "$UND" ]; then echo "CLOSED after $iter iterations"; echo "objs: $OBJS"; exit 0; fi
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
  if [ $ADDED -eq 0 ]; then echo "STUCK; remaining undefined:"; echo "$UND"; echo "objs: $OBJS"; exit 1; fi
done
echo "no convergence"; exit 1
