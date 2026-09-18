/*
 * Clr.c - ND-500 Clr instruction (MOVE class)
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
 * Clr instruction - MOVE class
 *
 * Clear Register to Zero: 0 -> Rn
 *
 * Variants: 6
 * Mnemonics: clr
 * Operands: 0 (no operands)
 *
 * Opcodes:
 *   0x0084 (BIn clr) bit register clear
 *   0x0084 (BYn clr) byte register clear
 *   0x0084 (Hn clr)  halfword register clear
 *   0x0084 (Wn clr)  word register clear
 *   0x0088 (Fn clr)  float register clear
 *   0x008C (Dn clr)  double float register clear
 *
 * Operation: 0 -> Rn
 *
 * Description:
 *   The register is set to all zeroes. For all integer data types,
 *   the entire register is cleared.
 *
 *   Register selection is encoded in opcode bits 1-0:
 *   - 00 -> register 1 (I1, A1)
 *   - 01 -> register 2 (I2, A2)
 *   - 10 -> register 3 (I3, A3)
 *   - 11 -> register 4 (I4, A4)
 *
 *   For integer variants: Uses I1-I4 registers
 *   For float/double: Uses A1-A4 (float) or D1-D4 (A+E pairs, double)
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 (always, result is zero)
 *   S = 0 (always, result is positive zero)
 *
 * Trap conditions:
 *   - None
 *
 * Reference: ND-500 Reference Manual, Chapter 10.16
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Clr.cs
 */
void nd500_instr_Clr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] Clr at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Clear target register to zero
    if (fi->uses_float_registers) {
        // Float/double: Write zero to A or D register
        nd500_write_float_register(cpu, fi->target_register, 0);
        if (fi->data_type == ND500_DTYPE_DOUBLEWORD) {
            // For double, also clear E register
            nd500_write_double_register(cpu, fi->target_register, 0);
        }
    } else {
        // Integer: Write zero to I register
        nd500_write_integer_register(cpu, fi->target_register, 0);
    }

    // Set Z flag to 1 (result is always zero)
    cpu->ST1 |= ND500_FLAG_Z;

    // S, C and O are not named ("1 -> Z" only), so they are reset (manual 6.5.1)
    cpu->ST1 &= ~(ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);
}
