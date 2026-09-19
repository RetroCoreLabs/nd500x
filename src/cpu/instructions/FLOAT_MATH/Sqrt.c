/*
 * Sqrt.c - ND-500 Sqrt instruction (FLOAT_MATH class)
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
#include "float_exact.h"
#include <stdio.h>

/**
 * Sqrt instruction - FLOAT_MATH class
 *
 * Variants: 8
 * Mnemonics: sqrt sqrt
 * Operands: 1
 *
 * Opcodes:
 *   0xFCD4-0xFCD7 (sqrt) - F1-F4 SQRT (float)
 *   0xFCD8-0xFCDB (sqrt) - D1-D4 SQRT (double)
 *
 * Calculates square root: sqrt(argument) -> register
 * Traps IVO if argument < 0.
 */
void nd500_instr_Sqrt(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] SQRT expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine if float or double from opcode */
    bool is_double = (fi->opcode >= 0xFCD8 && fi->opcode <= 0xFCDB);
    uint8_t reg_num = fi->target_register;

    if (reg_num < 1 || reg_num > 4) {
        printf("[ERROR] SQRT at PC=0x%08X: Invalid register %u\n",
               fi->address, reg_num);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint64_t arg_bits = nd500_read_float_operand(cpu, &fi->operands[0], is_double);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* The root is formed exactly and rounded once (float_exact.h). SQRTF
     * @001477 -> SQRTF_0 @020373 is a bit-by-bit ALU square root followed by
     * one rounding step; its results on 159 positive operands, run through
     * the ND5000 microword engine, are exactly the correctly rounded root.
     * SQRTD_100 @020423 is the same method on 64 bits, so the double result
     * is taken to be correctly rounded as well (inferred: the engine's double
     * path is not reliable enough to confirm it). A negative operand goes to
     * IVOZRO @020504: result 0 and the invalid operation trap. */
    unsigned exc = 0;
    uint64_t result_bits = nd500_fx_sqrt(arg_bits, is_double, &exc);
    nd500_write_float_reg(cpu, reg_num, result_bits, is_double);

    /* Z and S from the result; C and O are not named, so they are reset
     * (6.5.1; SQRTF_1 saves status with ST,SAVA). */
    nd500_set_flags_zs_float(cpu, result_bits, is_double);
    if (exc & ND500_FX_IVO) {
        trap_invalid_operation(cpu, fi->address);
    }
}
