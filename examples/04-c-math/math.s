# pcc2 : Version: 3.4
	.data
LL0:
	.text
_add:
	ents	$LFU1
	w move $8,b.16	#k1
	w1 := b.20	#q9
	w1 + b.24	#q9
	go	L12
L12:
	ret
	.set LFU1, 28
_sub:
	ents	$LFU2
	w move $8,b.16	#k1
	w1 := b.20	#q10
	w1 - b.24	#q10
	go	L14
L14:
	ret
	.set LFU2, 28
_main:
	ents	$LFU3
	w stz b.16	#k1
	w move $12,b.20	#k1
	w move $5,b.24	#k1
	w1 := b.20	#o5
	w1 * b.24	#o5
	w1 + $7	#q8
	w1 =: b.28	#k1
	w1 := b.20	#o5
	w1 / $2	#o5
	w2 := b.28	#q10
	w2 - r1	#q10
	w2 =: b.32	#k1
	w stz b.68	#k1
	w test b.32	#m2
	if >< go	L17:h
	w1 laddr b.36	#s1
	w1 + b.68	#q8
	by move $48,r1.0	#k1
	w incr b.68	#j1
	go	L18
L17:
	w move b.32,b.72	#k1
	w test b.72	#m2
	if >= go	L19:h
	w1 laddr b.36	#s1
	w1 + b.68	#q8
	by move $45,r1.0	#k1
	w incr b.68	#j1
	w1 := b.72	#m1
	w1 neg	#n1
	w1 =: b.72	#k1
L19:
	w stz b.100	#k1
L20:
	w test b.72	#m2
	if <= go	L21:h
	w1 div4 b.72,$10,b.aux	#o8
	w1 + $48	#q8
	w byconv r1,r1	#a4.2
	w2 laddr b.84	#s1
	w2 + b.100	#q8
	by1 =: r2.0	#k1
	w incr b.100	#j1
	w div2 b.72,$10	#o2
	go	L20
L21:
L22:
	w test b.100	#m2
	if <= go	L23:h
	w1 laddr b.84	#s1
	w decr b.100	#p2
	w1 + b.100	#q8
	w2 laddr b.36	#s1
	w2 + b.68	#q8
	by move r1.0,r2.0	#k1
	w incr b.68	#j1
	go	L22
L23:
L18:
	w1 laddr b.36	#s1
	w1 + b.68	#q8
	by move $10,r1.0	#k1
	w incr b.68	#j1
	w move $5,b.20+LFU3	#c2
	w move $7,b.24+LFU3	#c2
	call _add,$0	#h1
	w1 =: b.76	#k1
	w move b.76,b.20+LFU3	#c2
	w move $2,b.24+LFU3	#c2
	call _sub,$0	#h1
	w1 =: b.80	#k1
	w set1 b.20+LFU3	#c2
	w1 laddr b.36	#s1
	w1 =: b.24+LFU3	#c2
	w move b.68,b.28+LFU3	#c2
	call _write,$0	#h1
L16:
	ret
	.set LFU3, 104
	.data
	.globl	_add
	.globl	_sub
	.globl	_main
