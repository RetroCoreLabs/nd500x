# 1 "locore.c"










# 13 "locore.c"






	.globl	_kernel_main, _u, _Kstack
	.globl	_Textbase, _Physbase, _sharebase




	.set	_Textbase,	0x08000000	
	.set	_Physbase,	0x10000000	
	.set	_sharebase,	0x30000000	
	.set	_u,		0xe8000000	





	.set	_Kstack,	_u+0x1000	




	.set	UPAGES,		8		
	.set	NBPG,		2048		











	.text
	.org	4			
	.globl	start
start:
	








	init	_Kstack, $20, $UPAGES*NBPG

	







	dctsb
	pctsb

	





	



	call	_kernel_main, $0

	


halt_loop:
	go	halt_loop





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
	.stabs	"stack_area:G24=ar1;0;65535;2",0x20,0,1,0
	.comm	_stack_area,65536
	.stabs	"xmsg_cmd_buf:G25=ar1;0;2047;2",0x20,0,1,0
	.comm	_xmsg_cmd_buf,2048
	.stabs	"xmsg_resp_buf:G25",0x20,0,1,0
	.comm	_xmsg_resp_buf,2048
	.stabs	"clockrec:G1",0x20,0,4,0
	.comm	_clockrec,4
	.stabs	"console_in:G1",0x20,0,4,0
	.comm	_console_in,4
	.stabs	"console_out:G1",0x20,0,4,0
	.comm	_console_out,4
	.stabs	"kernel_main:F11",0x24,0,0,_kernel_main
	.text
	.file	"kernel.c"
_kernel_main:
	ents	$LFU1
	.stabd	0104,0,0236
	w stz b.16	#k1
	.stabd	0104,0,0237
	.stabd	0104,0,0240
	call _init_kernel,$0	#h1
	.stabd	0104,0,0241
	.stabd	0104,0,0242
	.stabd	0104,0,0243
	call _scheduler,$0	#h1
	.stabd	0104,0,0244
	.stabd	0104,0,0245
	.stabd	0104,0,0246
L40:
	.stabd	0104,0,0247
	go	L40
L41:
	.stabd	0104,0,0250
L39:
	ret
	.set LFU1, 20
	.stabs	"init_kernel:F11",0x24,0,0,_init_kernel
	.file	"kernel.c"
_init_kernel:
	ents	$LFU2
	.stabd	0104,0,0256
	w stz b.16	#k1
	.stabd	0104,0,0257
	.stabs	"i:1",0x80,0,4,20
	.stabd	0104,0,0260
	.stabd	0104,0,0261
	.stabd	0104,0,0262
	.stabd	0300,0,02
	w stz _nproc	#k1
	.stabd	0104,0,0263
	w stz _nfile	#k1
	.stabd	0104,0,0264
	w stz _ninode	#k1
	.stabd	0104,0,0265
	w stz _boottime	#k1
	.stabd	0104,0,0266
	.stabd	0104,0,0267
	.stabd	0104,0,0270
	call _init_proctab,$0	#h1
	.stabd	0104,0,0271
	call _init_filetab,$0	#h1
	.stabd	0104,0,0272
	call _init_inodetab,$0	#h1
	.stabd	0104,0,0273
	.stabd	0104,0,0274
	.stabd	0104,0,0275
	w stz b.20	#k1
L45:
	w comp2 b.20,$2048	#f3
	if >= go	L44:h
	.stabd	0104,0,0276
	w1 := b.20	#q9
	w1 + $_xmsg_cmd_buf	#q9
	by stz r1.0	#k1
	.stabd	0104,0,0277
	w1 := b.20	#q9
	w1 + $_xmsg_resp_buf	#q9
	by stz r1.0	#k1
	.stabd	0104,0,0300
L43:
	# opleaf : l3
	w incr b.20	#j1
	go	L45
L44:
	.stabd	0104,0,0301
	.stabd	0104,0,0302
	.stabd	0104,0,0303
	w stz _clockrec	#k1
	.stabd	0104,0,0304
	w stz _console_in	#k1
	.stabd	0104,0,0305
	w stz _console_out	#k1
	.stabd	0104,0,0306
	.stabd	0340,0,02
L42:
	ret
	.set LFU2, 24
	.stabs	"init_proctab:F11",0x24,0,0,_init_proctab
	.file	"kernel.c"
_init_proctab:
	ents	$LFU3
	.stabd	0104,0,0314
	w stz b.16	#k1
	.stabd	0104,0,0315
	.stabs	"i:1",0x80,0,4,20
	.stabd	0104,0,0316
	.stabd	0104,0,0317
	.stabd	0300,0,02
	w stz b.20	#k1
L49:
	w comp2 b.20,$100	#f3
	if >= go	L48:h
	.stabd	0104,0,0320
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0321
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+4	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0322
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+8	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0323
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+12	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0324
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+16	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0325
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+20	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0326
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+24	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0327
	w1 := b.20	#o5
	w1 * $32	#o5
	w1 + $_proctab+28	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0330
L47:
	# opleaf : l3
	w incr b.20	#j1
	go	L49
L48:
	.stabd	0104,0,0331
	.stabd	0104,0,0332
	.stabd	0104,0,0333
	w move $3,_proctab	#k1
	.stabd	0104,0,0334
	w stz _proctab+4	#k1
	.stabd	0104,0,0335
	w stz _proctab+8	#k1
	.stabd	0104,0,0336
	w set1 _nproc	#k1
	.stabd	0104,0,0337
	.stabd	0340,0,02
L46:
	ret
	.set LFU3, 24
	.stabs	"init_filetab:F11",0x24,0,0,_init_filetab
	.file	"kernel.c"
_init_filetab:
	ents	$LFU4
	.stabd	0104,0,0345
	w stz b.16	#k1
	.stabd	0104,0,0346
	.stabs	"i:1",0x80,0,4,20
	.stabd	0104,0,0347
	.stabd	0104,0,0350
	.stabd	0300,0,02
	w stz b.20	#k1
L53:
	w comp2 b.20,$128	#f3
	if >= go	L52:h
	.stabd	0104,0,0351
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0352
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+4	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0353
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+8	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0354
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+12	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0355
	w1 := b.20	#o5
	w1 * $20	#o5
	w1 + $_filetab+16	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0356
L51:
	# opleaf : l3
	w incr b.20	#j1
	go	L53
L52:
	.stabd	0104,0,0357
	.stabd	0340,0,02
L50:
	ret
	.set LFU4, 24
	.stabs	"init_inodetab:F11",0x24,0,0,_init_inodetab
	.file	"kernel.c"
_init_inodetab:
	ents	$LFU5
	.stabd	0104,0,0365
	w stz b.16	#k1
	.stabd	0104,0,0366
	.stabs	"i:1",0x80,0,4,20
	.stabd	0104,0,0367
	.stabd	0104,0,0370
	.stabd	0300,0,02
	w stz b.20	#k1
L57:
	w comp2 b.20,$64	#f3
	if >= go	L56:h
	.stabd	0104,0,0371
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0372
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+4	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0373
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+8	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0374
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+12	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0375
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+16	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0376
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+20	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0377
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+24	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0400
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+28	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0401
	w1 := b.20	#o5
	w1 * $36	#o5
	w1 + $_inodetab+32	#q8
	w stz r1.0	#k1
	.stabd	0104,0,0402
L55:
	# opleaf : l3
	w incr b.20	#j1
	go	L57
L56:
	.stabd	0104,0,0403
	.stabd	0340,0,02
L54:
	ret
	.set LFU5, 24
	.stabs	"sys_read:F1",0x24,0,4,_sys_read
	.stabs	"fd:p1",0xa0,0,4,20
	.stabs	"buf:p14",0xa0,0,1,24
	.stabs	"count:p1",0xa0,0,4,28
	.file	"kernel.c"
_sys_read:
	ents	$LFU6
	.stabd	0104,0,0414
	w move $12,b.16	#k1
	.stabd	0104,0,0415
	.stabd	0104,0,0416
	w stz r1	#m1
	go	L58
	.stabd	0104,0,0417
L58:
	ret
	.set LFU6, 32
	.stabs	"sys_write:F1",0x24,0,4,_sys_write
	.stabs	"fd:p1",0xa0,0,4,20
	.stabs	"buf:p14",0xa0,0,1,24
	.stabs	"count:p1",0xa0,0,4,28
	.file	"kernel.c"
_sys_write:
	ents	$LFU7
	.stabd	0104,0,0430
	w move $12,b.16	#k1
	.stabd	0104,0,0431
	.stabd	0104,0,0432
	w1 := b.28	#m1
	go	L59
	.stabd	0104,0,0433
L59:
	ret
	.set LFU7, 32
	.stabs	"sys_exit:F1",0x24,0,4,_sys_exit
	.stabs	"status:p1",0xa0,0,4,20
	.file	"kernel.c"
_sys_exit:
	ents	$LFU8
	.stabd	0104,0,0442
	w move $4,b.16	#k1
	.stabd	0104,0,0443
	.stabd	0104,0,0444
	w test _nproc	#m2
	if <= go	L61:h
	.stabd	0104,0,0445
	w move $5,_proctab	#k1
	.stabd	0104,0,0446
	.stabd	0104,0,0447
L61:
	w stz r1	#m1
	go	L60
	.stabd	0104,0,0450
L60:
	ret
	.set LFU8, 24
	.stabs	"scheduler:F11",0x24,0,0,_scheduler
	.file	"kernel.c"
_scheduler:
	ents	$LFU9
	.stabd	0104,0,0457
	w stz b.16	#k1
	.stabd	0104,0,0460
	.stabs	"i:1",0x80,0,4,20
	.stabd	0104,0,0461
	.stabs	"current:1",0x80,0,4,24
	.stabd	0104,0,0462
	.stabd	0104,0,0463
	.stabd	0300,0,02
	w stz b.24	#k1
	.stabd	0104,0,0464
	.stabd	0104,0,0465
	.stabd	0104,0,0466
L63:
	.stabd	0104,0,0467
	.stabd	0104,0,0470
	w stz b.20	#k1
L67:
	w comp2 b.20,$100	#f3
	if >= go	L66:h
	.stabd	0104,0,0471
	w1 := b.24	#q9
	w1 + $1	#q9
	w2 div4 r1,$100,b.aux	#o8
	w2 =: b.24	#k1
	.stabd	0104,0,0472
	.stabd	0104,0,0473
	w1 := b.24	#o5
	w1 * $32	#o5
	w1 + $_proctab	#q8
	w comp2 r1.0,$3	#f3
	if >< go	L68:h
	.stabd	0104,0,0474
	.stabd	0104,0,0475
	.stabd	0104,0,0476
	go	L66
	.stabd	0104,0,0477
	.stabd	0104,0,0500
L68:
L65:
	# opleaf : l3
	w incr b.20	#j1
	go	L67
L66:
	.stabd	0104,0,0501
	.stabd	0104,0,0502
	.stabd	0104,0,0503
	# opleaf : l3
	w incr _clockrec	#j1
	.stabd	0104,0,0504
	go	L63
L64:
	.stabd	0104,0,0505
	.stabd	0340,0,02
L62:
	ret
	.set LFU9, 28
	.stabs	"trap_handler:F11",0x24,0,0,_trap_handler
	.stabs	"trapno:p1",0xa0,0,4,20
	.file	"kernel.c"
_trap_handler:
	ents	$LFU10
	.stabd	0104,0,0514
	w move $4,b.16	#k1
	.stabd	0104,0,0515
	.stabd	0104,0,0516
	go	L69
	.stabd	0104,0,0517
L69:
	ret
	.set LFU10, 24
	.stabs	"main:F1",0x24,0,4,_main
	.file	"kernel.c"
_main:
	ents	$LFU11
	.stabd	0104,0,0526
	w stz b.16	#k1
	.stabd	0104,0,0527
	.stabd	0104,0,0530
	call _kernel_main,$0	#h1
	.stabd	0104,0,0531
	.stabd	0104,0,0532
	w stz r1	#m1
	go	L71
	.stabd	0104,0,0533
L71:
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
	.globl	_stack_area
	.globl	_xmsg_cmd_buf
	.globl	_xmsg_resp_buf
	.globl	_clockrec
	.globl	_console_in
	.globl	_console_out
	.globl	_kernel_main
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
