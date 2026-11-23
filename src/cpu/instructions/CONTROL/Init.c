/*
 * Init.c - ND-500 Init instruction (CONTROL class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include "nd500_settings.h"   /* emulator knobs, as plain fields */

/**
 * Init instruction - CONTROL class
 *
 * Initialize stack for program startup.
 *
 * Mnemonic: INIT
 * Operands: 3 (bottom of stack, stack demand main, total stack demand)
 * Opcode: 0x00DC
 *
 * Format: INIT <<bottom of stack/r/W>>, <stack demand main/r/W>, <total stack demand/r/W>
 *
 * Operation:
 *   <<bottom of stack>> -> B
 *   <<bottom of stack>> + <total stack demand> -> TOS
 *   <<bottom of stack>> + <stack demand main> -> B.SP
 *   0 -> B.PREVB
 *   0 -> B.RETA -> L
 *
 * Description:
 *   Initialize a new stack for the main program or initial module.
 *   This is typically the first instruction executed when a program starts.
 *
 *   The zeros in PREVB and RETA serve as sentinels:
 *   - RET with PREVB=0 triggers stack underflow trap
 *   - Prevents returning past the bottom of the call stack
 *
 *   Stack overflow occurs if bottom + <stack demand main> >= bottom + <total stack demand>
 *   (32-bit sums, unsigned compare). ND-05.009.4 13.9 compares the two DEMANDS
 *   ("<stack demand of main program> greater than or equal to <total system stack
 *   demand>"; 6.5.3.1 says "greater than"); the sums give the same answer unless a
 *   sum passes 2**32. The sums are NOT DOCUMENTED (taken from control store 002362B,
 *   not evidence; open in nd-lang docs/BUGS.md P7). The STO bit is always set or
 *   reset (6.5.3.1); the trap is taken only when enabled, otherwise the stack is
 *   still built (6.1: a disabled ignorable trap has no effect on execution).
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Stack overflow (STO) if stack demand main >= total stack demand
 *
 * Flags: Unaffected
 *
 * Reference:
 *   - ND-500 Reference Manual, Page 229 (Chapter 13.9)
 *   - Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CONTROL/Init.cs
 *
 * NOTE: Manual example on page 229 appears to have operands reversed.
 *       Correct: INIT FRAME, 0x1000, 0x10000 (main < total)
 *       Wrong:   INIT FRAME, 0x10000, 0x1000 (would trap!)
 */
void nd500_instr_Init(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Stack frame field offsets */
    const uint32_t OFFSET_PREVB = 0;   /* Previous B */
    const uint32_t OFFSET_RETA  = 4;   /* Return address */
    const uint32_t OFFSET_SP    = 8;   /* Stack pointer */

    /* Validate operand count */
    if (fi->operand_count != 3) {
        printf("[ERROR] INIT at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands (all should be WORD values) */
    uint32_t bottom_of_stack = nd500_read_operand_word(cpu, &fi->operands[0]);
    uint32_t stack_demand_main = nd500_read_operand_word(cpu, &fi->operands[1]);
    uint32_t total_stack_demand = nd500_read_operand_word(cpu, &fi->operands[2]);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }


    /* Stack overflow. What ND-05.009.4 says: 13.9 INIT "A value of <stack demand of
     * main program> greater than or equal to <total system stack demand> will cause a
     * stack overflow trap condition" (6.5.3.1 says "greater than"); 6.5.3.1 "The STO
     * status bit is set/reset for each ... INIT ..."; STO is Ignorable (Table 10) and
     * 6.1 "Ignorable trap conditions ... may be disabled and will have no effect on
     * program execution".
     * NOT DOCUMENTED (open, nd-lang docs/BUGS.md P7): comparing the two 32-bit SUMS
     * instead of the demands (differs only when a sum passes 2**32), and that with
     * STO enabled nothing is written. Both were taken from the control store
     * CONT-STORE-10611 (INIT body 012604B-012614B, which calls the check at 002362B
     * with JSR from 012606B), which is not evidence:
     *   012605B  AM#32 := bottom + stack demand main    (32-bit, wraps)
     *   012606B  AL#30 := bottom + total stack demand   (32-bit, wraps)
     *   002362B  AM#32 - AL#30, condition = carry        (carry: AM#32 >= AL#30, unsigned)
     *   002363B  S1 := S1 AND NOT bit 27 (STO)            no carry -> return, build the stack
     *   002364B  TE AND bit 27, condition = zero
     *   002365B  S1 := S1 OR bit 27 (STO)                 zero (STO not enabled) -> return,
     *                                                     enabled -> 000627B, the trap entry
     * So with STO enabled nothing is written; with STO NOT enabled the STO status bit
     * is set and the stack is built anyway (ND-05.009.4 6.5.3: a disabled ignorable
     * trap has no effect on program execution). Same pattern as ENTS/ENTSN. */
    int overflowed = ((uint32_t)(bottom_of_stack + stack_demand_main) >=
                      (uint32_t)(bottom_of_stack + total_stack_demand));
    if (overflowed) {
        ND500X_TRAPLOG("[TRAP] INIT at PC=0x%08X: Stack overflow - bottom+main 0x%08X >= bottom+total 0x%08X\n",
               fi->address, bottom_of_stack + stack_demand_main, bottom_of_stack + total_stack_demand);
        int enabled = nd500_ignorable_trap_enabled(cpu, TRAP_STO);
        /* Sets the STO status bit; dispatches only when enabled (002365B). */
        trap_stack_overflow(cpu, fi->address);
        if (enabled || nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
    }

    /* STEP 1: Set B to bottom of stack (like C# line 98) */
    cpu->B = bottom_of_stack;

    /* STEP 2: Set TOS (top of stack limit) (like C# line 101) */
    cpu->TOS = bottom_of_stack + total_stack_demand;

    /* STEP 3: Initialize stack frame header with sentinel values (like C# lines 104-106) */
    nd500_write_memory_32(cpu, cpu->B + OFFSET_PREVB, 0);  /* PREVB = 0 (bottom sentinel) */
    nd500_write_memory_32(cpu, cpu->B + OFFSET_RETA, 0);   /* RETA = 0 (bottom sentinel) */
    cpu->L = 0;  /* L also cleared */

    /* STEP 4: Set initial SP (like C# line 109) */
    nd500_write_memory_32(cpu, cpu->B + OFFSET_SP, bottom_of_stack + stack_demand_main);

    /* STO status bit is set/reset for each ENTS, ENTSN, ENTB, INIT, ENTM and
     * GETB (ND-500 Reference Manual, traps section). Successful completion
     * resets it - the bit must not stay stale after an earlier overflow. */
    if (!overflowed) {
        /* 002363B clears STO; 002365B sets it again on the overflow path. */
        cpu->ST1 &= ~(uint32_t)TRAP_STO;
    }

    /* NOTE: C# code lines 113-124 copy arguments, but manual spec does NOT mention this.
     * INIT is executed at program startup BEFORE any CALL, so no arguments should exist.
     * We omit argument copying to strictly follow the manual specification.
     */

    /* Diagnostic only - gated so it never leaks onto the SINTRAN console.
     * Set ND500X_INITLOG=1 to see the stack setup each INIT performs. */
    if (nd500_settings()->initlog) {
        printf("[INIT] Stack initialized at B=0x%08X, TOS=0x%08X, SP=0x%08X\n",
               cpu->B, cpu->TOS, bottom_of_stack + stack_demand_main);
    }
}
