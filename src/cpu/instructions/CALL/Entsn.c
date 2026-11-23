/*
 * Entsn.c - ND-500 Entsn instruction (CALL class)
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

/**
 * Entsn instruction - CALL class
 *
 * Enter Stack Subroutine with Maximum Arguments - Like ENTS but limits
 * the number of arguments transferred to the stack frame.
 *
 * Mnemonic: ENTSN
 * Operands: 2 (stack demand in BYTES - per ND-05.009.4 Section 13.10, max argument count)
 * Opcode: 0x00BA
 *
 * Format: ENTSN <stack_demand>, <max_args>
 *
 * Stack Frame Layout (same as ENTS):
 *   +0   PREVB    Previous B register value
 *   +4   RETA     Return address
 *   +8   SP       Stack pointer (B + stack_demand)
 *   +12  AUX      Auxiliary field (0 for ENTSN)
 *   +16  N        Argument count (LIMITED to max_args!)
 *   +20  ARG1     First argument address
 *   +24  ARG2     Second argument address
 *   ...  ...      (up to max_args arguments)
 *
 * Operation:
 *   Same as ENTS, but B.N = min(actual_args, max_args)
 *   Only the first max_args arguments are copied to the stack frame.
 *
 * Use cases:
 *   - Variable-argument functions (e.g., printf)
 *   - Functions with optional parameters
 *   - Protecting stack from excessive argument counts
 *
 * Example:
 *   CALL PRINTF, 5, FMT, A1, A2, A3, A4    ; 5 args provided
 *   PRINTF:
 *     ENTSN 200H, 3                        ; Accept max 3
 *     ; B.N = 3 (not 5!)
 *     ; Only FMT, A1, A2 are in stack frame
 *
 * Traps:
 *   - ISE (Instruction Sequence Error) if CALL did not precede
 *   - STO (Stack Overflow) if stack demand exceeds available space
 *
 * Reference: ND-500 Reference Manual and docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/CALL/Entsn.cs
 */
void nd500_instr_Entsn(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Stack frame field offsets */
    const uint32_t OFFSET_PREVB = 0;   /* Previous B */
    const uint32_t OFFSET_RETA  = 4;   /* Return address */
    const uint32_t OFFSET_SP    = 8;   /* Stack pointer */
    const uint32_t OFFSET_AUX   = 12;  /* Auxiliary */
    const uint32_t OFFSET_N     = 16;  /* Argument count */
    const uint32_t OFFSET_ARG1  = 20;  /* First argument address */

    /* Validate that CALL preceded this instruction */
    if (cpu->pending_call_return_address == 0) {
        ND500X_TRAPLOG("[TRAP] ENTSN at PC=0x%08X: Must be preceded by CALL/CALLG\n",
               fi->address);
        trap_instruction_sequence_error(cpu, fi->address);
        return;
    }

    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] ENTSN at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read stack demand operand (already in bytes per ND-05.009.4 Section 13.10) */
    uint32_t stack_demand = nd500_read_operand_word(cpu, &fi->operands[0]);

    /* Read maximum argument count (operand 1) */
    uint32_t max_args = nd500_read_operand_word(cpu, &fi->operands[1]);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Read old B.SP to get new B */
    uint32_t old_b = cpu->B;
    uint32_t new_b = nd500_read_memory_32(cpu, old_b + OFFSET_SP);

    /* A fault on that read leaves the value garbage; abort before it is used. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Stack overflow. What ND-05.009.4 says:
     *   13.10 ENTSN: "similar to ENTS"; ENTS: "There will be a stack overflow trap
     *     condition if B + <stack demand> is greater than or equal to TOS".
     *   6.5.3.1 STO: "The STO status bit is set/reset for each ENTS, ENTSN, ENTB,
     *     INIT, ENTM and GETB instruction."
     *   Table 10 / 6.5.7: STO is an Ignorable trap, timing D (during); 6.1:
     *     "Ignorable trap conditions ... may be disabled and will have no effect on
     *     program execution" - so with STO not enabled the frame is built.
     * NOT DOCUMENTED (open, see nd-lang docs/BUGS.md P7): that the compare is
     *   unsigned 32-bit, and that with STO enabled NOTHING is written before the
     *   trap (6.5.7 "During": call and enter instructions may be "partially
     *   executed before the trap occurs"). Taken from the control store
     *   CONT-STORE-10611 (ENTS entry 000324B; ENTSN joins the same words at
     *   002416B), which is not evidence:
     *   002420B  AM#34 := new B + stack demand
     *   002421B  AM#34 - TOS, condition = carry      (carry: AM#34 >= TOS, unsigned)
     *   002422B  S1 := S1 AND NOT bit 27 (STO)       no carry -> 002425B, build the frame
     *   002423B  TE AND bit 27, condition = zero
     *   002424B  S1 := S1 OR bit 27 (STO)            zero (STO not enabled) -> 002425B,
     *                                                enabled -> 000627B, the trap entry
     * So with STO enabled the instruction traps before it writes anything; with STO
     * NOT enabled the STO status bit is set and the frame is built anyway, past TOS
     * (ND-05.009.4 6.5.3: a disabled ignorable trap has no effect on program
     * execution). Skipping the frame here made a program that recurses past its
     * stack loop for ever in the caller's frame (nd-lang tutorial T1014). */
    int overflowed = (new_b + stack_demand >= cpu->TOS);
    if (overflowed) {
        ND500X_TRAPLOG("[TRAP] ENTSN at PC=0x%08X: Stack overflow (newB=0x%08X, demand=0x%08X, TOS=0x%08X)\n",
               fi->address, new_b, stack_demand, cpu->TOS);
        int enabled = nd500_ignorable_trap_enabled(cpu, TRAP_STO);
        /* Sets the STO status bit; dispatches only when enabled (002424B). */
        trap_stack_overflow(cpu, fi->address);
        if (enabled || nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
    }

    /* Limit argument count to maximum */
    uint32_t actual_arg_count = cpu->pending_call_arg_count;
    uint32_t transfer_count = (actual_arg_count < max_args) ? actual_arg_count : max_args;

    /* Initialize new stack frame */
    /* B.PREVB = old B */
    nd500_write_memory_32(cpu, new_b + OFFSET_PREVB, old_b);

    /* B.RETA = return address */
    nd500_write_memory_32(cpu, new_b + OFFSET_RETA, cpu->pending_call_return_address);

    /* B.SP = new B + stack demand */
    nd500_write_memory_32(cpu, new_b + OFFSET_SP, new_b + stack_demand);

    /* B.AUX = 0 (not used for ENTSN) */
    /* B.AUX (offset 12) is NOT written by the entry instruction: ND-05.009.4 lists PREVB, RETA/L,
       SP, N and the argument addresses only. AUX belongs to the language runtime and must survive
       a re-entry of the same frame - PLANC carries the ROUTINEERROR code there (measured 2026-09-26,
       CONVERT-DOM-A03 vs the CONT-STORE-10611 microcode; RetroCore B30). This used to write 0. */

    /* B.N = ACTUAL transferred count (not caller's count!) */
    nd500_write_memory_32(cpu, new_b + OFFSET_N, transfer_count);

    /* Copy only the first max_args argument addresses */
    for (uint32_t i = 0; i < transfer_count && i < TRAP_SEQ_MAXARG; i++) {
        nd500_write_memory_32(cpu, new_b + OFFSET_ARG1 + (i * 4),
                             cpu->pending_call_arg_addresses[i]);
    }

    /* Update B register to new stack frame */

    /* A memory fault on any access above must abort BEFORE the commit below: the
     * real machine loads L and releases the CALL/ENT* sequence interlock only in
     * the TERMINAL microword (MICRO-5800-B30 ENTS_END @004206 loads L via
     * D,DAC,REG05; ENTSN_3 @004254 asserts C,SEQ / INVSEQ), both alongside the
     * final WRITE and the exit to the next instruction. Earlier frame writes are
     * separate microwords, so a fault there leaves L and the interlock untouched
     * and the retried entry instruction still sees its CALL. Without this the
     * retry raises a FALSE ISE - the defect that killed vi through ENTS. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    cpu->B = new_b;

    /* Update L register with return address */
    cpu->L = cpu->pending_call_return_address;

    /* Clear pending call state */
    cpu->pending_call_return_address = 0;
    cpu->pending_call_arg_count = 0;

    /* STO status bit is set/reset for each ENTS, ENTSN, ENTB, INIT, ENTM and
     * GETB (ND-500 Reference Manual, traps section). Successful completion
     * resets it - the bit must not stay stale after an earlier overflow. */
    if (!overflowed) {
        /* 002422B clears STO; 002424B sets it again on the overflow path. */
        cpu->ST1 &= ~(uint32_t)TRAP_STO;
    }

    /* PC already advanced by cpu_step() */
}
