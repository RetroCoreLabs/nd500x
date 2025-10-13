#!/bin/bash
# Test corpus: Compare nd500x disassembly vs nd500-dis
set -e

OBJFILES=("$@")
if [ ${#OBJFILES[@]} -eq 0 ]; then
    OBJFILES=(/mnt/f/add.o /mnt/f/multiply.o)
fi

TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

for obj in "${OBJFILES[@]}"; do
    if [ ! -f "$obj" ]; then
        echo "Skip missing: $obj"
        continue
    fi
    base=$(basename "$obj")
    echo "=== Testing $base ==="
    
    # Get text size from a.out header
    textsize=$(od -An -tu4 -N4 -j4 "$obj" | awk '{print $1}')
    
    # Run nd500x disasm
    ../build/bin/nd500x -i "$obj" --disasm "$textsize" --addr 0 > "$TMPDIR/${base}.ours" 2>&1 || true
    
    # Run reference nd500-dis
    nd500-dis "$obj" > "$TMPDIR/${base}.ref" 2>&1 || true
    
    # Filter out header/metadata lines for comparison
    grep -vE '^(===|loaded|Magic|Text |Data |BSS|Symbol|Entry|reloc|Header|String|;|File:|Text segment|Data segment)' "$TMPDIR/${base}.ours" | sed '/^$/d' | sed 's/^[0-9A-Fa-f]\{8\}: //' | sed -E 's/^([0-9A-F][0-9A-F] )+  *//' | sed 's/^        //' > "$TMPDIR/${base}.ours.clean"
    grep -vE '^;' "$TMPDIR/${base}.ref" | sed '/^$/d' | sed 's/^        //' > "$TMPDIR/${base}.ref.clean"
    
    # Compare
    if diff -u "$TMPDIR/${base}.ref.clean" "$TMPDIR/${base}.ours.clean" > "$TMPDIR/${base}.diff"; then
        echo "✓ PASS: $base"
    else
        echo "✗ FAIL: $base"
        cat "$TMPDIR/${base}.diff"
    fi
done

