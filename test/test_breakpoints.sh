#!/bin/bash
# Test breakpoint and watchpoint functionality

echo "=== Testing ND500X Debugger Enhancements ==="
echo ""
echo "Test 1: Setting and listing breakpoints"
echo "Test 2: Enabling/disabling breakpoints"
echo "Test 3: Setting and listing watchpoints"
echo "Test 4: Deleting breakpoints and watchpoints"
echo ""

# Create test script for interactive session
cat > /tmp/nd500x_test_bp.txt << 'EOF'
# Set multiple breakpoints
bp 0x100
bp 0x200
bp 0x300

# List breakpoints
bp list

# Disable one breakpoint
bp disable 1

# List again
bp list

# Set watchpoints
wp 0x1000 4 write
wp 0x2000 8 read
wp 0x3000 4 change

# List watchpoints
wp list

# Delete a breakpoint
bp del 0

# Delete a watchpoint
wp del 1

# Final state
bp list
wp list

# Exit
q
EOF

# Run test
cd /home/ronny/repos/nd500x
./build/bin/nd500x --debug < /tmp/nd500x_test_bp.txt

# Cleanup
rm -f /tmp/nd500x_test_bp.txt

echo ""
echo "=== Test Complete ==="

