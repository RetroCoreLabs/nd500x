# Test LGet (l=:) instruction
# Store L register to memory and verify

	.text
	.globl	_start
_start:
	# Set L register to a known value via I1
	w1 := $0xDEADBEEF
	l := r1

	# Store L to register (test 1)
	l=: r2

	# Store L to local memory b.0 (test 2)
	l=: b.0

	# Halt
	bp
