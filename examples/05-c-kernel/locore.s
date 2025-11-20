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





