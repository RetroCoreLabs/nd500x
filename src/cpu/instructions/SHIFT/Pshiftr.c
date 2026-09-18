/*
 * Pshiftr.c - ND-500 Pshiftr instruction (SHIFT class)
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
 * Pshiftr instruction - SHIFT class
 *
 * PSHIFTR - Packed (BCD) shift, ROUNDED (Manual 17.6)
 *
 * Format: PSHIFTR <source/r/BCD=>, <dest/w/BCD=>
 * Opcode: 0xFE87 / 177207B (single BCD-option instruction, 2 operands)
 *
 * PSHIFTR is the SAME packed-decimal scale operation as PSHIFT, except that when
 * decimal positions are removed (destination scaling factor smaller than the
 * source's) the result is ROUNDED rather than truncated. It is NOT a binary
 * logical bit shift (the prior implementation was a plain `value >> count`, a
 * different, non-existent instruction).
 *
 * Operation (manual 17.6):
 *   Re-scale the packed-decimal <source> to the scaling factor of <dest>,
 *   rounding (half away from zero) any digits dropped, and store into <dest>.
 *   Equal scaling factors -> MOVE. Destination is zero-extended.
 *
 * Data Status Bits (manual 17.6, rule 4040 for C/O):
 *   Z  <- (value after rounding == 0)
 *   S  <- value sign bit
 *   BO <- BCD overflow (set by the write helper)
 *   K  <- BO OR IVO (set by the write helper on overflow/invalid)
 *   C, O cleared.
 *
 * Traps: Addressing; BCD overflow (BO); Invalid operation (IVO).
 *
 * Reference: ND-500 Reference Manual, Section 17.6.
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SHIFT/Pshiftr.cs
 */
void nd500_instr_Pshiftr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] PSHIFTR at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* operand[0] = source BCD descriptor; operand[1] = destination BCD descriptor. */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t dest_desc_addr = fi->operands[1].effective_address;

    Nd500StringDescriptor source_desc, dest_desc;
    if (!nd500_load_string_descriptor(cpu, source_desc_addr, true, false, &source_desc)) {
        return;  /* invalid descriptor - K flag set by helper */
    }
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, true, false, &dest_desc)) {
        return;
    }

    /* Read the packed-decimal source (unscaled integer representation). */
    int64_t source_value = nd500_read_packed_bcd_value(cpu, &source_desc);

    /* Re-scale source -> dest scaling factor with ROUNDING (half away from zero).
     * The helper rounds when dest scaling drops decimal positions; BO/K are set
     * on BCD overflow. */
    nd500_write_packed_bcd_value_rounded(cpu, &dest_desc, source_value,
                                         source_desc.scaling_factor);

    /* Flags: read back the stored destination value for Z/S. */
    int64_t result_value = nd500_read_packed_bcd_value(cpu, &dest_desc);
    if (result_value == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }
    if (result_value < 0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* C and O not named for PSHIFTR -> cleared (rule 4040). BO/K handled by the
     * write helper. */
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
