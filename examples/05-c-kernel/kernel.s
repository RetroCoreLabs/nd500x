# pcc2 : Version: 3.4
	.data
	.stabs	"kernel.c",0144,0,0,LL0
LL0:
	.text
	.stabs	"int:t1=r1;-2147483648;2147483647;",0x80,0,0,0
	.stabs	"char:t2=r2;0;127;",0x80,0,0,0
	.stabs	"long:t3=r1;-2147483648;2147483647;",0x80,0,0,0
	.stabs	"short:t4=r1;-32768;32767;",0x80,0,0,0
	.stabs	"unsigned char:t5=r1;0;255;",0x80,0,0,0
	.stabs	"unsigned short:t6=r1;0;65535;",0x80,0,0,0
	.stabs	"unsigned long:t7=r1;0;-1;",0x80,0,0,0
	.stabs	"unsigned int:t8=r1;0;-1;",0x80,0,0,0
	.stabs	"float:t9=r1;4;0;",0x80,0,0,0
	.stabs	"double:t10=r1;8;0;",0x80,0,0,0
	.stabs	"void:t11=11",0x80,0,0,0
	.stabs	"???:t12=1",0x80,0,0,0
	.stabs	"proc:T13=s32p_stat:1,0,32;p_pid:1,32,32;p_ppid:1,64,32;p_pri:1,96,32;p_cpu:1,128,32;p_nice:1,160,32;p_flag:1,192,32;p_addr:14=*2,224,32;;",0x80,0,32,-1275
	.stabs	"file:T15=s20f_flag:1,0,32;f_type:1,32,32;f_count:1,64,32;f_offset:1,96,32;f_inode:17=*16,128,32;;",0x80,0,20,-1275
	.stabs	"inode:T16=s36i_flag:1,0,32;i_count:1,32,32;i_dev:1,64,32;i_number:1,96,32;i_mode:1,128,32;i_nlink:1,160,32;i_uid:1,192,32;i_gid:1,224,32;i_size:1,256,32;;",0x80,0,36,-1275
	.stabs	"buf:T18=s28b_flags:1,0,32;b_forw:19=*18,32,32;b_back:19,64,32;b_dev:1,96,32;b_blkno:1,128,32;b_addr:14,160,32;b_bcount:1,192,32;;",0x80,0,28,-1275
	.stabs	"proctab:G20=ar1;0;99;13",0x20,0,32,0
	.comm	_proctab,3200
	.stabs	"filetab:G21=ar1;0;127;15",0x20,0,20,0
	.comm	_filetab,2560
	.stabs	"inodetab:G22=ar1;0;63;16",0x20,0,36,0
	.comm	_inodetab,2304
	.stabs	"buftab:G23=ar1;0;31;18",0x20,0,28,0
	.comm	_buftab,896
	.stabs	"nproc:G1",0x20,0,4,0
	.data
_nproc:
	.long	0	#b1
	.stabs	"nfile:G1",0x20,0,4,0
	.data
_nfile:
	.long	0	#b1
	.stabs	"ninode:G1",0x20,0,4,0
	.data
_ninode:
	.long	0	#b1
	.stabs	"ncpu:G1",0x20,0,4,0
	.data
_ncpu:
	.long	1	#b1
	.stabs	"boottime:G1",0x20,0,4,0
	.data
_boottime:
	.long	0	#b1
	.stabs	"hz:G1",0x20,0,4,0
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
	.stabs	"xmsg_cmd_buf:G24=ar1;0;2047;2",0x20,0,1,0
	.comm	_xmsg_cmd_buf,2048
	.stabs	"xmsg_resp_buf:G24",0x20,0,1,0
	.comm	_xmsg_resp_buf,2048
	.stabs	"clockrec:G1",0x20,0,4,0
	.comm	_clockrec,4
	.stabs	"console_in:G1",0x20,0,4,0
	.comm	_console_in,4
	.stabs	"console_out:G1",0x20,0,4,0
	.comm	_console_out,4
	.stabs	"start:F11",0x24,0,0,_start
	.text
_start:
	ents	$LFU1
	.stabd	0104,0,0230
	w stz b.16	#k1
	.stabd	0104,0,0231
	.stabd	0104,0,0232
	call _init_kernel,$0	#h1
	.stabd	0104,0,0233
	.stabd	0104,0,0234
	.stabd	0104,0,0235
	call _scheduler,$0	#h1
	.stabd	0104,0,0236
	.stabd	0104,0,0237
	.stabd	0104,0,0240
L38:
	.stabd	0104,0,0241
	go	L38
L39:
	.stabd	0104,0,0242
L37:
	ret
	.set LFU1, 20
	.stabs	"init_kernel:F11",0x24,0,0,_init_kernel
_init_kernel:
	ents	$LFU2
	.stabd	0104,0,0250
	w stz b.16	#k1
	.stabd	0104,0,0251
	.stabs	"i:1",0x80,0,4,20
	.stabd	0104,0,0252
	.stabd	0104,0,0253
	.stabd	0104,0,0254
	.stabd	0300,0,02
	w stz _nproc	#k1
	.stabd	0104,0,0255
	w stz _nfile	#k1
	.stabd	0104,0,0256
	w stz _ninode	#k1
	.stabd	0104,0,0257
	w stz _boottime	#k1
	.stabd	0104,0,0260
	.stabd	0104,0,0261
	.stabd	0104,0,0262
	call _init_proctab,$0	#h1
	.stabd	0104,0,0263
	call _init_filetab,$0	#h1
	.stabd	0104,0,0264
	call _init_inodetab,$0	#h1
	.stabd	0104,0,0265
	.stabd	0104,0,0266
	.stabd	0104,0,0267
	w stz b.20	#k1
L43:
	w comp2 b.20,$2048	#f3
	if >= go	L42:h
	.stabd	0104,0,0270
	w1 := b.20	#q9
	w1 + $_xmsg_cmd_buf	#q9
	by stz r1.0	#k1
	.stabd	0104,0,0271
	w1 := b.20	#q9
	w1 + $_xmsg_resp_buf	#q9
	by stz r1.0	#k1
	.stabd	0104,0,0272
L41:
	# opleaf : l3
	w incr b.20	#j1
	go	L43
L42:
	.stabd	0104,0,0273
	.stabd	0104,0,0274
	.stabd	0104,0,0275
	w stz _clockrec	#k1
	.stabd	0104,0,0276
	w stz _console_in	#k1
	.stabd	0104,0,0277
	w stz _console_out	#k1
	.stabd	0104,0,0300
	.stabd	0340,0,02
L40:
	ret
	.set LFU2, 24
	.stabs	"init_proctab:F11",0x24,0,0,_init_proctab
_init_proctab:
	ents	$LFU3
	.stabd	0104,0,0306
	w stz b.16	#k1
	.stabd	0104,0,0307
	.stabs	"i:1",0x80,0,4,20
	.stabd	0104,0,0310
	.stabd	0104,0,0311
	.stabd	0300,0,02
	w stz b.20	#k1
L47:
	w comp2 b.20,$100	#f3
	if >= go	L46:h
	.stabd	0104,0,0312
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0313
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+4	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0314
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+8	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0315
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+12	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0316
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+16	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0317
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+20	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0320
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+24	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0321
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+28	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0322
L45:
	# opleaf : l3
	w incr b.20	#j1
	go	L47
L46:
	.stabd	0104,0,0323
	.stabd	0104,0,0324
	.stabd	0104,0,0325
	w move $3,_proctab	#k1
	.stabd	0104,0,0326
	w stz _proctab+4	#k1
	.stabd	0104,0,0327
	w stz _proctab+8	#k1
	.stabd	0104,0,0330
	w set1 _nproc	#k1
	.stabd	0104,0,0331
	.stabd	0340,0,02
L44:
	ret
	.set LFU3, 24
	.stabs	"init_filetab:F11",0x24,0,0,_init_filetab
_init_filetab:
	ents	$LFU4
	.stabd	0104,0,0337
	w stz b.16	#k1
	.stabd	0104,0,0340
	.stabs	"i:1",0x80,0,4,20
	.stabd	0104,0,0341
	.stabd	0104,0,0342
	.stabd	0300,0,02
	w stz b.20	#k1
L51:
	w comp2 b.20,$128	#f3
	if >= go	L50:h
	.stabd	0104,0,0343
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0344
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+4	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0345
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+8	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0346
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+12	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0347
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+16	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0350
L49:
	# opleaf : l3
	w incr b.20	#j1
	go	L51
L50:
	.stabd	0104,0,0351
	.stabd	0340,0,02
L48:
	ret
	.set LFU4, 24
	.stabs	"init_inodetab:F11",0x24,0,0,_init_inodetab
_init_inodetab:
	ents	$LFU5
	.stabd	0104,0,0357
	w stz b.16	#k1
	.stabd	0104,0,0360
	.stabs	"i:1",0x80,0,4,20
	.stabd	0104,0,0361
	.stabd	0104,0,0362
	.stabd	0300,0,02
	w stz b.20	#k1
L55:
	w comp2 b.20,$64	#f3
	if >= go	L54:h
	.stabd	0104,0,0363
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0364
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+4	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0365
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+8	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0366
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+12	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0367
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+16	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0370
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+20	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0371
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+24	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0372
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+28	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0373
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+32	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0374
L53:
	# opleaf : l3
	w incr b.20	#j1
	go	L55
L54:
	.stabd	0104,0,0375
	.stabd	0340,0,02
L52:
	ret
	.set LFU5, 24
	.stabs	"sys_read:F1",0x24,0,4,_sys_read
	.stabs	"fd:p1",0xa0,0,4,20
	.stabs	"buf:p14",0xa0,0,1,24
	.stabs	"count:p1",0xa0,0,4,28
_sys_read:
	ents	$LFU6
	.stabd	0104,0,0406
	w move $12,b.16	#k1
	.stabd	0104,0,0407
	.stabd	0104,0,0410
	w stz r1	#m1
	go	L56
	.stabd	0104,0,0411
L56:
	ret
	.set LFU6, 32
	.stabs	"sys_write:F1",0x24,0,4,_sys_write
	.stabs	"fd:p1",0xa0,0,4,20
	.stabs	"buf:p14",0xa0,0,1,24
	.stabs	"count:p1",0xa0,0,4,28
_sys_write:
	ents	$LFU7
	.stabd	0104,0,0422
	w move $12,b.16	#k1
	.stabd	0104,0,0423
	.stabd	0104,0,0424
	w1 := b.28	#m1
	go	L57
	.stabd	0104,0,0425
L57:
	ret
	.set LFU7, 32
	.stabs	"sys_exit:F1",0x24,0,4,_sys_exit
	.stabs	"status:p1",0xa0,0,4,20
_sys_exit:
	ents	$LFU8
	.stabd	0104,0,0434
	w move $4,b.16	#k1
	.stabd	0104,0,0435
	.stabd	0104,0,0436
	w test _nproc	#m2
	if <= go	L59:h
	.stabd	0104,0,0437
	w move $5,_proctab	#k1
	.stabd	0104,0,0440
	.stabd	0104,0,0441
L59:
	w stz r1	#m1
	go	L58
	.stabd	0104,0,0442
L58:
	ret
	.set LFU8, 24
	.stabs	"scheduler:F11",0x24,0,0,_scheduler
_scheduler:
	ents	$LFU9
	.stabd	0104,0,0451
	w stz b.16	#k1
	.stabd	0104,0,0452
	.stabs	"i:1",0x80,0,4,20
	.stabd	0104,0,0453
	.stabs	"current:1",0x80,0,4,24
	.stabd	0104,0,0454
	.stabd	0104,0,0455
	.stabd	0300,0,02
	w stz b.24	#k1
	.stabd	0104,0,0456
	.stabd	0104,0,0457
	.stabd	0104,0,0460
L61:
	.stabd	0104,0,0461
	.stabd	0104,0,0462
	w stz b.20	#k1
L65:
	w comp2 b.20,$100	#f3
	if >= go	L64:h
	.stabd	0104,0,0463
	w1 := b.24	#q9
	w1 + $1	#q9
	w2 div4 r1,$100,b.aux	#o8
	w2 =: b.24	#k1
	.stabd	0104,0,0464
	.stabd	0104,0,0465
	w1 := b.24	#o5
	w1 * $32	#o5
	w1 + $_proctab	#q8
	w comp2 r1.0,$3	#f3
	if >< go	L66:h
	.stabd	0104,0,0466
	.stabd	0104,0,0467
	.stabd	0104,0,0470
	go	L64
	.stabd	0104,0,0471
	.stabd	0104,0,0472
L66:
L63:
	# opleaf : l3
	w incr b.20	#j1
	go	L65
L64:
	.stabd	0104,0,0473
	.stabd	0104,0,0474
	.stabd	0104,0,0475
	# opleaf : l3
	w incr _clockrec	#j1
	.stabd	0104,0,0476
	go	L61
L62:
	.stabd	0104,0,0477
	.stabd	0340,0,02
L60:
	ret
	.set LFU9, 28
	.stabs	"trap_handler:F11",0x24,0,0,_trap_handler
	.stabs	"trapno:p1",0xa0,0,4,20
_trap_handler:
	ents	$LFU10
	.stabd	0104,0,0506
	w move $4,b.16	#k1
	.stabd	0104,0,0507
	.stabd	0104,0,0510
	go	L67
	.stabd	0104,0,0511
L67:
	ret
	.set LFU10, 24
	.stabs	"main:F1",0x24,0,4,_main
_main:
	ents	$LFU11
	.stabd	0104,0,0520
	w stz b.16	#k1
	.stabd	0104,0,0521
	.stabd	0104,0,0522
	call _start,$0	#h1
	.stabd	0104,0,0523
	.stabd	0104,0,0524
	w stz r1	#m1
	go	L69
	.stabd	0104,0,0525
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
