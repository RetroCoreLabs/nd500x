.text
.org	4
.globl	_start
_start:
	w1 := $42
	w2 := $100
	w3 := r1
	w3 + r2
	w4 := r2
	w4 - r1
	w1 := $0
	w2 := $-1
	bp
