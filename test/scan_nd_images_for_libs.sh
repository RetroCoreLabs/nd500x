#!/bin/bash
# Search every ND disk image for a GENUINE NC-LIB / CAT-LIB (C runtime libraries).
# "NC-LIB" must be excluded when it is merely a substring of PLANC-LIB.
out=/tmp/claude-1000/lib_scan_results.txt
: > "$out"
n=0
find /mnt/d/ND \( -iname "*.img" -o -iname "*.IMG" \) 2>/dev/null | sort | while read -r f; do
  n=$((n+1))
  # -a: treat binary as text. Match CAT-LIB anywhere, or NC-LIB not preceded by a letter.
  hits=$(grep -aoiE "CAT-LIB[A-Z0-9-]*|(^|[^A-Za-z])NC-LIB[A-Z0-9-]*" "$f" 2>/dev/null \
         | sed 's/^[^A-Za-z]//' | sort -u | tr '\n' ' ')
  if [ -n "$hits" ]; then
    echo "HIT: $f -> $hits" >> "$out"
  fi
done
echo "SCAN COMPLETE: $(find /mnt/d/ND \( -iname '*.img' -o -iname '*.IMG' \) 2>/dev/null | wc -l) images searched" >> "$out"
