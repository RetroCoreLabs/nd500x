#!/bin/bash
# Test relocation comment display for unresolved externals

echo "=== Testing Relocation Comments ==="
echo ""
echo "Test: Disassemble object file with unresolved external (_write)"
echo ""

# Derive the repo root from this script's own location.
cd "$(dirname "$0")/.."

: "${PCC_ND500:?set PCC_ND500 to your pcc-nd500 checkout}"

# Test with math.o which has unresolved _write symbol
echo "Loading math.o and disassembling call instruction..."
./build/bin/nd500x --debug << EOF | grep -E "(call|UNRESOLVED)"
load $PCC_ND500/examples/04-c-math/math.o
d 0x100 10
q
EOF

echo ""
echo "Expected output: call         \$0,\$0 ; _write (UNRESOLVED)"
echo ""
echo "=== Test Complete ==="

