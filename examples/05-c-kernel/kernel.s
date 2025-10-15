# pcc2 : Version: 3.4
	.data
LL0:
	.text
	.comm	_proctab,3200
	.comm	_filetab,2560
	.comm	_inodetab,2304
	.comm	_buftab,896
	.data
_nproc:
	.long	0	#b1
	.data
_nfile:
	.long	0	#b1
	.data
_ninode:
	.long	0	#b1
	.data
_ncpu:
	.long	1	#b1
	.data
_boottime:
	.long	0	#b1
	.data
_hz:
	.long	100	#b1
	.data
_version:
	.byte	78
	.byte	68
	.byte	73
	.byte	88
	.byte	45
	.byte	67
	.byte	32
	.byte	83
	.byte	105
	.byte	109
	.byte	117
	.byte	108
	.byte	97
	.byte	116
	.byte	101
	.byte	100
	.byte	32
	.byte	75
	.byte	101
	.byte	114
	.byte	110
	.byte	101
	.byte	108
	.byte	32
	.byte	118
	.byte	49
	.byte	46
	.byte	48
	.byte	32
	.byte	102
	.byte	111
	.byte	114
	.byte	32
	.byte	78
	.byte	68
	.byte	45
	.byte	53
	.byte	48
	.byte	48
	.byte	10
	.byte	0
	.comm	_xmsg_cmd_buf,2048
	.comm	_xmsg_resp_buf,2048
	.comm	_clockrec,4
	.comm	_console_in,4
	.comm	_console_out,4
	.text
_start:
	ents	$LFU1
	w stz b.16	#k1
	call _init_kernel,$0	#h1
	call _scheduler,$0	#h1
L38:
	go	L38
L39:
L37:
	ret
	.set LFU1, 20
_init_kernel:
	ents	$LFU2
	w stz b.16	#k1
	w stz _nproc	#k1
	w stz _nfile	#k1
	w stz _ninode	#k1
	w stz _boottime	#k1
	call _init_proctab,$0	#h1
	call _init_filetab,$0	#h1
	call _init_inodetab,$0	#h1
	w stz b.20	#k1
L43:
	w comp2 b.20,$2048	#f3
	if >= go	L42:h
	w1 := b.20	#q9
	w1 + $_xmsg_cmd_buf	#q9
	by stz r1.0	#k1
	w1 := b.20	#q9
	w1 + $_xmsg_resp_buf	#q9
	by stz r1.0	#k1
L41:
	# opleaf : l3
	w incr b.20	#j1
	go	L43
L42:
	w stz _clockrec	#k1
	w stz _console_in	#k1
	w stz _console_out	#k1
L40:
	ret
	.set LFU2, 24
_init_proctab:
	ents	$LFU3
	w stz b.16	#k1
	w stz b.20	#k1
L47:
	w comp2 b.20,$100	#f3
	if >= go	L46:h
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+4	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+8	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+12	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+16	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+20	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+24	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+28	#q8
	w stz r1.0	#k1
L45:
	# opleaf : l3
	w incr b.20	#j1
	go	L47
L46:
	w move $3,_proctab	#k1
	w stz _proctab+4	#k1
	w stz _proctab+8	#k1
	w set1 _nproc	#k1
L44:
	ret
	.set LFU3, 24
_init_filetab:
	ents	$LFU4
	w stz b.16	#k1
	w stz b.20	#k1
L51:
	w comp2 b.20,$128	#f3
	if >= go	L50:h
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+4	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+8	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+12	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+16	#q8
	w stz r1.0	#k1
L49:
	# opleaf : l3
	w incr b.20	#j1
	go	L51
L50:
L48:
	ret
	.set LFU4, 24
_init_inodetab:
	ents	$LFU5
	w stz b.16	#k1
	w stz b.20	#k1
L55:
	w comp2 b.20,$64	#f3
	if >= go	L54:h
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+4	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+8	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+12	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+16	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+20	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+24	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+28	#q8
	w stz r1.0	#k1
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+32	#q8
	w stz r1.0	#k1
L53:
	# opleaf : l3
	w incr b.20	#j1
	go	L55
L54:
L52:
	ret
	.set LFU5, 24
_sys_read:
	ents	$LFU6
	w move $12,b.16	#k1
	w stz r1	#m1
	go	L56
L56:
	ret
	.set LFU6, 32
_sys_write:
	ents	$LFU7
	w move $12,b.16	#k1
	w1 := b.28	#m1
	go	L57
L57:
	ret
	.set LFU7, 32
_sys_exit:
	ents	$LFU8
	w move $4,b.16	#k1
	w test _nproc	#m2
	if <= go	L59:h
	w move $5,_proctab	#k1
L59:
	w stz r1	#m1
	go	L58
L58:
	ret
	.set LFU8, 24
_scheduler:
	ents	$LFU9
	w stz b.16	#k1
	w stz b.24	#k1
L61:
	w stz b.20	#k1
L65:
	w comp2 b.20,$100	#f3
	if >= go	L64:h
	w1 := b.24	#q9
	w1 + $1	#q9
	w2 div4 r1,$100,b.aux	#o8
	w2 =: b.24	#k1
	w1 := b.24	#o5
	w1 * $32	#o5
	w1 + $_proctab	#q8
	w comp2 r1.0,$3	#f3
	if >< go	L66:h
	go	L64
L66:
L63:
	# opleaf : l3
	w incr b.20	#j1
	go	L65
L64:
	# opleaf : l3
	w incr _clockrec	#j1
	go	L61
L62:
L60:
	ret
	.set LFU9, 28
_trap_handler:
	ents	$LFU10
	w move $4,b.16	#k1
	go	L67
L67:
	ret
	.set LFU10, 24
_main:
	ents	$LFU11
	w stz b.16	#k1
	call _start,$0	#h1
	w stz r1	#m1
	go	L69
L69:
	ret
	.set LFU11, 20
	.data
	.globl	_proctab
	.globl	_filetab
	.globl	_inodetab
	.globl	_buftab
	.globl	_nproc
	.globl	_nfile
	.globl	_ninode
	.globl	_ncpu
	.globl	_boottime
	.globl	_hz
	.globl	_version
	.globl	_xmsg_cmd_buf
	.globl	_xmsg_resp_buf
	.globl	_clockrec
	.globl	_console_in
	.globl	_console_out
	.globl	_start
	.globl	_init_kernel
	.globl	_init_proctab
	.globl	_init_filetab
	.globl	_init_inodetab
	.globl	_sys_read
	.globl	_sys_write
	.globl	_sys_exit
	.globl	_scheduler
	.globl	_trap_handler
	.globl	_main
