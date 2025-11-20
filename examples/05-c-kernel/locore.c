/*
 * ND-500 Kernel Bootstrap (locore.c)
 *
 * Low-level assembly bootstrap code for ND-500 kernel.
 * This file uses the assembly-in-C pattern from NDIX-C to solve
 * the ENTS-without-INIT problem.
 *
 * Based on NDIX-C Release 3 kernel/MASTER/machine/locore.c
 */

#ifndef LOCORE
#define LOCORE 1
#endif

/*
 * Kernel memory layout constants
 * These match the NDIX-C memory map for future vmunix integration
 */
	.globl	_kernel_main, _u, _Kstack
	.globl	_Textbase, _Physbase, _sharebase

/*
 * Virtual address space layout (NDIX-C compatible)
 */
	.set	_Textbase,	0x08000000	/* Kernel text segment base */
	.set	_Physbase,	0x10000000	/* Physical memory / data segment */
	.set	_sharebase,	0x30000000	/* Shared memory with ND-100 */
	.set	_u,		0xe8000000	/* Current process U-area */

/*
 * Kernel stack location within U-area
 * NDIX places stack at offset 0xf00, we use similar layout
 */
	.set	_Kstack,	_u+0x1000	/* Kernel stack (4KB into U-area) */

/*
 * Stack and U-area sizing
 */
	.set	UPAGES,		8		/* Pages per U-area (8 * 2KB = 16KB) */
	.set	NBPG,		2048		/* Bytes per page (ND-500 page size) */

/*
 * System Entry Point
 *
 * This is the first code executed when the kernel is loaded.
 * It must reside at logical text address 4 (after a.out magic number).
 *
 * CRITICAL: The INIT instruction MUST be the first instruction executed
 * to avoid "instruction sequence error trap" when entering functions
 * (ENTS requires prior stack initialization via INIT or CALL).
 */
	.text
	.org	4			/* Skip a.out magic number at offset 0 */
	.globl	start
start:
	/*
	 * Initialize stack registers (B, TOS, SP) and create initial frame
	 * This MUST come before any function calls (including ENTS)
	 *
	 * INIT syntax: init base, demand, size
	 *   base   = _Kstack (stack bottom address)
	 *   demand = $20 (32 bytes minimum frame)
	 *   size   = total stack space available
	 */
	init	_Kstack, $20, $UPAGES*NBPG

	/*
	 * Cache operations (from NDIX pattern)
	 * dctsb = Data Cache Test and Set (implies data cache clear)
	 * pctsb = Program Cache Test and Set (implies program cache clear)
	 *
	 * These ensure clean cache state before kernel initialization.
	 * If ND500X doesn't implement caches, these are no-ops.
	 */
	dctsb
	pctsb

	/*
	 * Clear U-area to ensure clean process context
	 * NDIX does: d bmove $0,_u,$(_Kstack-_u)/8
	 * We simplify since our U-area is minimal
	 */

	/*
	 * Call C kernel main entry point
	 * Stack is now properly initialized, safe to call C functions
	 */
	call	_kernel_main, $0

	/*
	 * If kernel_main() returns (should never happen), halt
	 */
halt_loop:
	go	halt_loop

/*
 * End of bootstrap code
 * All other kernel functionality is in kernel.c (C code)
 */
