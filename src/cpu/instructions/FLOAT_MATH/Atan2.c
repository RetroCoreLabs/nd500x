/*
 * Atan2.c - ND-500 Atan2 instruction (FLOAT_MATH class)
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
#include "float_math.h"
#include <stdio.h>

/**
 * Atan2 instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: atan2 atan2
 * Operands: 2
 *
 * Opcodes:
 *   0xFF70-0xFF73 (atan2) - F1-F4 ATAN2 (float)
 *   0xFF9C-0xFF9F (atan2) - D1-D4 ATAN2 (double)
 *
 * Calculates 2-argument arc tangent: atan2(y, x) -> register
 * Result in radians (-pi to pi).
 * Traps IVO if both y=0 and x=0.
 */
void nd500_instr_Atan2(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] ATAN2 expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine if float or double from opcode */
    bool is_double = (fi->opcode >= 0xFF9C && fi->opcode <= 0xFF9F);
    uint8_t reg_num = fi->target_register;

    if (reg_num < 1 || reg_num > 4) {
        printf("[ERROR] ATAN2 at PC=0x%08X: Invalid register %u\n",
               fi->address, reg_num);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint64_t y_bits = nd500_read_float_operand(cpu, &fi->operands[0], is_double);
    uint64_t x_bits = nd500_read_float_operand(cpu, &fi->operands[1], is_double);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    /* F and D are both computed the way the B30 microcode computes them
     * (float_math.h: same constants, reduction, polynomial and order of
     * AAP operations), not with the host maths library. */
    unsigned exc = 0;
    uint64_t r = nd500_fm_atan2(y_bits, x_bits, is_double, &exc);
    nd500_fm_store(cpu, fi->address, reg_num, r, exc, is_double);
}
