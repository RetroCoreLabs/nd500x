/*
 * Set1.c - ND-500 Set1 instruction (CONTROL class)
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
 * Set1 instruction - CONTROL class
 *
 * Set to One: 1 -> <operand>
 *
 * Variants: 6
 * Mnemonics: set1
 * Operands: 1 (<operand/w/t>)
 *
 * Opcodes:
 *   0xFC86 (BI set1 - bit)
 *   0xFC87 (BY set1 - byte)
 *   0xFC88 (H set1 - halfword)
 *   0x004D (W set1 - word)
 *   0x0047 (F set1 - float)
 *   0xFC89 (D set1 - double)
 *
 * Operation: 1 -> <operand>
 *
 * Description:
 *   The contents of the destination operand are replaced by one.
 *   This instruction sets a value to 1 regardless of its previous content.
 *
 *   Common uses:
 *   - Initializing counters
 *   - Setting boolean flags
 *   - Resetting accumulators to 1
 *
 * Flags: All cleared
 *   Z = 0
 *   S = 0
 *   C = 0
 *   O = 0
 *
 * Trap conditions:
 *   - Addressing traps
 *
 * Reference: ND-500 Reference Manual, Chapter 10.18
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/MOVE/Set1.cs
 */
void nd500_instr_Set1(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] Set1 at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    // Determine data type based on opcode
    Nd500DataType dtype;
    switch (fi->opcode) {
        case 0xFC86: dtype = ND500_DTYPE_BIT; break;         // BI set1 - sets single bit
        case 0xFC87: dtype = ND500_DTYPE_BYTE; break;        // BY set1
        case 0xFC88: dtype = ND500_DTYPE_HALFWORD; break;    // H set1
        case 0x004D: dtype = ND500_DTYPE_WORD; break;        // W set1
        case 0x0047: dtype = ND500_DTYPE_WORD; break;        // F set1 (32-bit float)
        case 0xFC89: dtype = ND500_DTYPE_DOUBLEWORD; break;  // D set1 (64-bit double)
        default:
            printf("[ERROR] Set1 at PC=0x%08X: Unknown opcode 0x%04X\n",
                   fi->address, fi->opcode);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    // Write 1 to destination operand.
    // Microcode SET1F @000326 / SET1D @000330 produce floating 1.0 for the F/D
    // variants (ALU constant 1 in the operand's datatype), NOT the integer bit
    // pattern 0x00000001 (which is a tiny denormal, not 1.0).
    if (fi->opcode == 0x0047) {          // F set1 -> single-precision 1.0
        nd500_write_operand_from_ieee_float(cpu, &fi->operands[0], 1.0, false);
    } else if (fi->opcode == 0xFC89) {   // D set1 -> double-precision 1.0
        nd500_write_operand_from_ieee_float(cpu, &fi->operands[0], 1.0, true);
    } else {
        nd500_write_operand_value(cpu, &fi->operands[0], 1, dtype);
    }

    // Clear all status flags (Z, S, C, O) - result +1 is positive, non-zero
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O);
}
