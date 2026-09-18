/*
 * Cind.c - ND-500 Cind instruction (SYSTEM class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Cind instruction - SYSTEM class
 *
 * CIND - Calculate Index (bounds check)
 *
 * Format: tn CIND <index/r/t>, <lower/r/t>, <upper/r/t>
 *
 * Assembly:
 *   BYn CIND (byte calculate index)            Hex 0xFD14+(n-1)
 *   Hn  CIND (halfword calculate index)        Hex 0xFD18+(n-1)
 *   Wn  CIND (word calculate index)            Hex 0xB0+(n-1)
 *   Fn  CIND (floating calculate index)        Hex 0xFFD0+(n-1)
 *   Dn  CIND (double float calculate index)    Hex 0xFFD4+(n-1)
 *
 * Operation:
 *   if <index> < <lower> or <index> > <upper> then
 *     1 -> K
 *     1 -> IX (Illegal Index trap status bit)
 *   else
 *     0 -> K
 *     0 -> IX
 *   endif
 *
 * Description:
 *   The <index> operand is checked to ensure it is within the bounds
 *   defined by <lower> and <upper> operands. If the index is out of
 *   bounds, the K flag and IX (Illegal Index) status bit are set.
 *   This instruction is used for array bounds checking.
 *
 * Trap conditions: Addressing traps
 *
 * Data status bits: K (out of bounds), IX trap bit (out of bounds)
 *
 * Reference: ND-500 Reference Manual, Chapter 15.9
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SYSTEM/Cind.cs
 */
void nd500_instr_Cind(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 54-58) */
    if (fi->operand_count != 3) {
        printf("[ERROR] CIND at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands (like C# lines 61-63) */
    uint64_t index = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    uint64_t lower = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    uint64_t upper = nd500_read_operand_value(cpu, &fi->operands[2], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* MULTI-DIMENSIONAL INDEX ACCUMULATION (the part the functional core was
     * missing): In := In*(upper-lower+1) + index, In being the accumulator held
     * in the target register Rn. Mirrors the real B30 microcode CIND_W
     * (@027116-027121): SC7 = upper-lower+1; SC5 = In*SC7 (AAP2,IMUL); SC5 += index
     * (A+B) -> written back to In; the final ST,SAVA/ACCM/ACCA latch Z/S. The
     * functional core previously computed no result and set no Z/S, leaving every
     * CIND golden's In and status stale (SYSTEM_cind sweep). Mirrors Cind.cs.
     * [CIND @027116-027121] */
    if (fi->target_register >= 1 && fi->target_register <= 4) {
        uint64_t in_acc = nd500_read_integer_register(cpu, fi->target_register);
        uint64_t range  = upper - lower + 1;
        uint64_t result = in_acc * range + index;
        /* MASK the result to the datatype before writing In (verified vs microword:
         * BY CIND 0x55555555 -> In=0x00000055, H CIND -> 0x00005555). CIND masks its
         * typed D,ALU,REG37 write, unlike LIND which stores verbatim. [CIND @027116-027121] */
        uint32_t masked = nd500_mask_to_datatype(result, fi->data_type);
        nd500_write_integer_register(cpu, fi->target_register, masked);
        nd500_set_flags_zs(cpu, masked, fi->data_type);
    }

    /* For signed comparison, sign-extend based on data type */
    int64_t sindex = nd500_sign_extend_by_dtype(index, fi->data_type);
    int64_t slower = nd500_sign_extend_by_dtype(lower, fi->data_type);
    int64_t supper = nd500_sign_extend_by_dtype(upper, fi->data_type);

    /* Check if index is within bounds (lower <= index <= upper) (like C# line 66) */
    bool out_of_bounds = (sindex < slower) || (sindex > supper);

    /* Set status bits based on bounds check (like C# lines 69-70) */
    /* IX (Illegal Index) trap bit is at position 26 in ST1 */
    #define ND500_ST_IX (1u << 26)
    if (out_of_bounds) {
        cpu->ST1 |= ND500_FLAG_K;  /* K flag */
        cpu->ST1 |= ND500_ST_IX;   /* IX trap bit (bit 26 in ST1) */
    } else {
        cpu->ST1 &= ~ND500_FLAG_K;
        cpu->ST1 &= ~ND500_ST_IX;
    }
}
