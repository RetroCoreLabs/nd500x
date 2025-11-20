# ND-500 Kernel Bootstrap

	.stabs	"bootstrap:F11",0x24,0,0,_bootstrap
	.text
	.file	"bootstrap.s"
_bootstrap:
	init	stack_bottom,$0x1000,$0x10000
	call	_start,$0
_halt:
	go	_halt

	.bss
stack_area:
	.space	0x10000

	.data
stack_bottom:
	.long	stack_area
