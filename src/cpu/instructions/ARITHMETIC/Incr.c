/*
 * Incr.c - ND-500 Incr instruction (ARITHMETIC class)
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
#include <math.h>

/**
 * Incr instruction - ARITHMETIC class
 *
 * Increment operand by one. operand = operand + 1
 *
 * Variants: 5 (by data type)
 * Mnemonics: BY INCR, H INCR, W INCR, F INCR, D INCR
 * Operands: 1 (value to increment, must be read-write)
 *
 * Opcodes:
 *   0xFC8A (BY INCR) - Byte increment
 *   0x004E (H INCR) - Halfword increment
 *   0x004F (W INCR) - Word increment
 *   0x0050 (F INCR) - Float increment
 *   0xFC8B (D INCR) - Double increment
 *
 * Operation: operand <- operand + 1
 *
 * Description:
 *   The operand is incremented by one. The Carry bit is set if a carry
 *   occurs from the sign bit position of the adder, otherwise it is reset.
 *   Carry will occur when and only when integer -1 is incremented.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if carry from most significant bit (only when -1 incremented)
 *   O = 1 if signed overflow occurred (max positive value incremented)
 *
 * Traps: Addressing traps, Integer overflow (O)
 *
 * Reference: ND-500 Reference Manual and docs/instructions/asm/ (authoritative).
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Incr.cs
 */
/* Appendix I, INCR row (ND-05.009.4 page 403): IVO, DZ, FU, FO and BO are
 * "unconditionally cleared"; 6.5.1: "All data status bits not mentioned are
 * reset". BO is bit 15 (6.5.1 table 7). Same as RetroCore Incr.cs. */
static void nd500_incr_clear_trap_bits(Nd500Cpu* cpu) {
    nd500_clear_flag(cpu, ND500_FLAG_IVO);
    nd500_clear_flag(cpu, ND500_FLAG_DZ);
    nd500_clear_flag(cpu, ND500_FLAG_FU);
    nd500_clear_flag(cpu, ND500_FLAG_FO);
    nd500_clear_flag(cpu, 1u << 15);
}

void nd500_instr_Incr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 1) {
        printf("[ERROR] INCR at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double variants */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);

        /* Read operand value */
        uint64_t value = 0;
        if (is_double) {
            uint64_t op_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }
            value = op_bits;
        } else {
            uint32_t op_bits = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }
            value = op_bits;
        }

        /* <operand> + 1 (10.19): one floating add rounded by 7.2.7 in the
         * ND-500 format itself. Until 2026-11-24 this went through a host
         * double (53 bits against D's 55) and a host float (24 against F's 23).
         * Same as RetroCore Incr.cs. */
        /* The exception word is not used: by 7.2.7 the largest F or D value
         * plus 1 rounds back to the largest value (guard bit 0), so the sum
         * never overflows, and no operand plus 1 underflows. */
        unsigned exc = 0;
        uint64_t result_bits = nd500_fx_add(value, is_double ? 0x4040000000000000ull : 0x40400000ull,
                                            is_double, &exc);
        (void)exc;

        /* Write back */
        if (is_double) {
            nd500_write_operand_value(cpu, &fi->operands[0], result_bits, ND500_DTYPE_DOUBLEWORD);
        } else {
            nd500_write_operand_value(cpu, &fi->operands[0], (uint32_t)result_bits, fi->data_type);
        }

        /* C and O stay clear on a float INCR: 10.19 "C (integer)"; 6.5.1
         * "Integer Overflow may be set only when performing integer
         * arithmetic; otherwise it is cleared"; Appendix I INCR row O = I. */
        nd500_clear_flag(cpu, ND500_FLAG_C);
        nd500_clear_flag(cpu, ND500_FLAG_O);
        nd500_incr_clear_trap_bits(cpu);

        /* Update flags: Z (zero), S (sign) */
        if (is_double) {
            if (nd500_double_is_zero(result_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_Z);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_Z);
            }
            if (nd500_double_is_negative(result_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_S);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_S);
            }
        } else {
            uint32_t float_bits = (uint32_t)result_bits;
            if (nd500_float_is_zero(float_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_Z);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_Z);
            }
            if (nd500_float_is_negative(float_bits)) {
                nd500_set_flag(cpu, ND500_FLAG_S);
            } else {
                nd500_clear_flag(cpu, ND500_FLAG_S);
            }
        }
        return;
    }

    /* Read operand value (like C# ReadOperandValue) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Perform increment */
    uint64_t result = value + 1;

    /* Detect carry and overflow before masking */
    bool carry = false;
    bool overflow = false;
    if (fi->data_type == ND500_DTYPE_BYTE) {
        carry = ((uint8_t)value == 0xFF);      /* Carry when -1 incremented */
        overflow = ((int8_t)value == 0x7F);     /* Overflow when max positive incremented */
    } else if (fi->data_type == ND500_DTYPE_HALFWORD) {
        carry = ((uint16_t)value == 0xFFFF);
        overflow = ((int16_t)value == 0x7FFF);
    } else if (fi->data_type == ND500_DTYPE_WORD) {
        carry = ((uint32_t)value == 0xFFFFFFFF);
        overflow = ((int32_t)value == 0x7FFFFFFF);
    }

    /* Mask to data type (like C# MaskToDataType) */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write result back to operand (like C# WriteOperandValue) */
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    /* Update status flags: Z, S, C, O */
    nd500_set_flags_zsco(cpu, masked_result, fi->data_type, carry, overflow);
    nd500_incr_clear_trap_bits(cpu);
}
