# ND-500 Kernel Bootstrap
# Proper stack initialization before calling kernel entry point
# Reference: ND-500 Reference Manual, Page 229 (INIT instruction)

	.text
	.global _bootstrap
	.global _start

# ═══════════════════════════════════════════════════════════════════
# BOOTSTRAP ENTRY POINT
# ═══════════════════════════════════════════════════════════════════
# This is the TRUE entry point after CPU reset or binary load.
# PC should be set to _bootstrap before execution starts.
#
# The INIT instruction initializes the stack according to ND-500 spec:
#   - Sets B register to stack bottom
#   - Sets TOS register to stack top
#   - Creates initial stack frame with PREVB=0, RETA=0
#   - Allows subsequent CALL/ENTS instructions to work correctly
# ═══════════════════════════════════════════════════════════════════

_bootstrap:
	# INIT <bottom of stack>, <main stack demand>, <total stack demand>
	#
	# Stack layout:
	#   stack_bottom (0x00010000) = 64KB offset
	#   Main program needs: 4KB (0x1000 bytes)
	#   Total stack space: 64KB (0x10000 bytes)
	#
	# After INIT:
	#   B = 0x00010000 (stack bottom)
	#   B.SP = 0x00011000 (bottom + 4KB for main)
	#   TOS = 0x00020000 (bottom + 64KB total)
	#   B.PREVB = 0 (no previous frame)
	#   B.RETA = 0 (no return address)
	#   L = 0

	init	stack_bottom, 0x1000, 0x10000

	# Now that stack is initialized, we can CALL the kernel entry point
	# CALL sets up pending_call_return_address, allowing ENTS to work
	call	_start, 0

	# If kernel returns (it shouldn't), halt in infinite loop
_halt:
	go	_halt

# ═══════════════════════════════════════════════════════════════════
# STACK AREA DEFINITION
# ═══════════════════════════════════════════════════════════════════
# Define the stack area in the .bss section (uninitialized data)
# Total size: 64KB (0x10000 bytes)
# ═══════════════════════════════════════════════════════════════════

	.bss
	.align 4
stack_area:
	.space	0x10000		# 64KB stack space

	.data
stack_bottom:
	.long	stack_area	# Absolute address of stack bottom

# ═══════════════════════════════════════════════════════════════════
# NOTES FOR EMULATOR USERS
# ═══════════════════════════════════════════════════════════════════
#
# 1. Set PC to _bootstrap before running:
#    (debugger) set PC <address of _bootstrap>
#    (debugger) run
#
# 2. Alternative: Load binary and let loader set entry point to _bootstrap
#
# 3. The INIT instruction is REQUIRED before any CALL/ENTS sequence
#    Without INIT, ENTS will trap with "Stack Overflow" or "ISE"
#
# 4. Stack overflow check: newB + demand >= TOS
#    With these settings: 0x10000 + demand < 0x20000 (OK if demand < 64KB)
#
# 5. Reference: ND-500 Reference Manual, Page 229-230
# ═══════════════════════════════════════════════════════════════════
