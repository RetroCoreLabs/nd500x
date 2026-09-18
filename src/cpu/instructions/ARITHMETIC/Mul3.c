/*
 * Mul3.c - ND-500 Mul3 instruction (ARITHMETIC class)
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
 * Mul3 instruction - ARITHMETIC class
 *
 * Extended Multiply (Three Operands): <a> * <b> -> <c>
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn MUL3, Hn MUL3, Wn MUL3, Fn MUL3, Dn MUL3 (n=1..4)
 * Operands: 3 (<a/r/t>, <b/r/t>, <c/w/t>)
 *
 * Opcodes:
 *   0xFC64-0xFC67 (BY1 MUL3 through BY4 MUL3) - Byte extended multiply
 *   0xFC68-0xFC6B (H1 MUL3 through H4 MUL3) - Halfword extended multiply
 *   0x009C-0x009F (W1 MUL3 through W4 MUL3) - Word extended multiply
 *   0x00A0-0x00A3 (F1 MUL3 through F4 MUL3) - Float extended multiply
 *   0x00A4-0x00A7 (D1 MUL3 through D4 MUL3) - Double extended multiply
 *
 * Operation: <a> * <b> -> <c>
 *
 * Description:
 *   The <a> operand is multiplied by the <b> operand and the result
 *   is stored in the <c> operand location. This is a three-operand
 *   version that allows multiplication without affecting any register contents.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 0 (multiplication doesn't set carry)
 *   O = 1 if overflow
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Extended Arithmetic)
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Mul3.cs
 */
void nd500_instr_Mul3(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count (like C# lines 54-58) */
    if (fi->operand_count != 3) {
        printf("[ERROR] MUL3 at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double types.
     * Microcode MUL3F / MUL3D: a * b -> <c>; ST,SAVF sets Z,S,FU,FO; C,O cleared (rule 4040). */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        double aValue = nd500_read_operand_as_ieee_float(cpu, &fi->operands[0], is_double);
        double bValue = nd500_read_operand_as_ieee_float(cpu, &fi->operands[1], is_double);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        double fresult = aValue * bValue;
        nd500_write_operand_from_ieee_float(cpu, &fi->operands[2], fresult, is_double);
        nd500_float_finish(cpu, fi->address, fresult, is_double);
        return;
    }

    uint64_t aValue, bValue, result;
    bool overflow = false;

    /* Read operand a value (like C# line 61) */
    aValue = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    /* Read operand b value (like C# line 64) */
    bValue = nd500_read_operand_value(cpu, &fi->operands[1], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Perform multiplication: a * b (like C# lines 66-99) */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            /* Signed byte multiplication (like C# lines 72-79) */
            int8_t aByte = (int8_t)(aValue & 0xFF);
            int8_t bByte = (int8_t)(bValue & 0xFF);
            int32_t product = (int32_t)aByte * (int32_t)bByte;
            result = (uint64_t)(uint8_t)(product & 0xFF);
            overflow = (product < -128 || product > 127);
            break;
        }

        case ND500_DTYPE_HALFWORD: {
            /* Signed halfword multiplication (like C# lines 82-89) */
            int16_t aHalf = (int16_t)(aValue & 0xFFFF);
            int16_t bHalf = (int16_t)(bValue & 0xFFFF);
            int32_t product = (int32_t)aHalf * (int32_t)bHalf;
            result = (uint64_t)(uint16_t)(product & 0xFFFF);
            overflow = (product < -32768 || product > 32767);
            break;
        }

        case ND500_DTYPE_WORD: {
            /* Signed word multiplication (like C# lines 92-99) */
            int32_t aWord = (int32_t)(aValue & 0xFFFFFFFF);
            int32_t bWord = (int32_t)(bValue & 0xFFFFFFFF);
            int64_t product = (int64_t)aWord * (int64_t)bWord;
            result = (uint64_t)(uint32_t)(product & 0xFFFFFFFF);
            overflow = (product < INT32_MIN || product > INT32_MAX);
            break;
        }

        default:
            printf("[ERROR] MUL3 at PC=0x%08X: Unsupported data type %d\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Write result to operand c location (like C# line 150) */
    nd500_write_operand_value(cpu, &fi->operands[2], result, fi->data_type);

    /* Update status flags based on result (like C# lines 152-156) */
    /* Set Z and S flags */
    nd500_set_flags_zs(cpu, result, fi->data_type);

    /* Clear carry flag - multiplication doesn't set carry (like C# line 155) */
    cpu->ST1 &= ~ND500_FLAG_C;

    /* Set or clear overflow flag */
    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    /* Handle trap conditions (like C# lines 158-170) */
    if (overflow) {
        ND500X_TRAPLOG("[TRAP] MUL3 at PC=0x%08X: Integer overflow\n", fi->address);
        trap_integer_overflow(cpu, fi->address);
        return;
    }
}
