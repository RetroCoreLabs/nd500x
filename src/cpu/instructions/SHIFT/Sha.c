/*
 * Sha.c - ND-500 Sha instruction (SHIFT class)
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
 * Sha instruction - SHIFT class
 *
 * Shift Arithmetic. Preserves sign bit during shift.
 *
 * Variants: 3 (by data type: BY, H, W; no BI/F/D variants exist)
 * Mnemonics: BY SHA, H SHA, W SHA
 * Operands: 2 (value to shift, shift count)
 *
 * Opcodes (manual 10.25: 176253B-176255B):
 *   0xFCAB (BY SHA) - Byte arithmetic shift
 *   0xFCAC (H SHA)  - Halfword arithmetic shift
 *   0xFCAD (W SHA)  - Word arithmetic shift
 *
 * Operation:
 *   If shift_count >= 0: operand << shift_count -> operand (sign preserved)
 *   If shift_count < 0:  operand >> abs(shift_count) -> operand (sign extended)
 *
 * Description:
 *   Arithmetic shift that preserves the sign bit.
 *   - Left shift: same as logical shift left
 *   - Right shift: sign bit is extended (replicated into vacated positions)
 *   Used for signed integer multiplication/division by powers of 2.
 *   Shift count is a signed byte (-127 to +127).
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *
 * Trap conditions:
 *   - IOV (Illegal Operand Value) if abs(shift_count) >= data_width_in_bits
 *
 * Reference: ND-500 Reference Manual, Chapter 10.25
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SHIFT/Sha.cs
 */
void nd500_instr_Sha(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] SHA at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands - read full register/memory value without data type masking */
    uint64_t raw_value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }
    int8_t shift_count = (int8_t)nd500_read_operand_byte(cpu, &fi->operands[1]); /* Signed byte */
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    /* Calculate bit width from data type (like C# line 34) */
    uint32_t bits;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:      bits = 8; break;
        case ND500_DTYPE_HALFWORD:  bits = 16; break;
        case ND500_DTYPE_WORD:      bits = 32; break;
        case ND500_DTYPE_DOUBLEWORD: bits = 64; break;
        default:                    bits = 32; break;
    }

    /* Get absolute shift (like C# line 35) */
    int32_t abs_shift = (shift_count >= 0) ? shift_count : -shift_count;

    /* Validate shift count (like C# lines 37-41) */
    if (abs_shift >= (int32_t)bits) {
        ND500X_TRAPLOG("[TRAP] SHA at PC=0x%08X: Illegal shift count %d (>= %u bits)\n",
               fi->address, abs_shift, bits);
        /* Raise IOV (Illegal Operand Value) trap - must be TRAP_IOV, not
         * trap_invalid_operation()'s TRAP_IVO - matches C# Sha.cs. */
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    /* Perform arithmetic shift (matching C# implementation):
     * - Positive count = LEFT shift (same as logical shift left)
     * - Negative count = RIGHT shift (arithmetic, sign-extending)
     */
    uint64_t result;
    if (shift_count >= 0) {
        /* Left shift: same as logical shift left */
        result = raw_value << abs_shift;
    } else {
        /* Right shift: arithmetic (sign-extending based on data type width) */
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE: {
                int8_t signed_val = (int8_t)(raw_value & 0xFF);
                result = (uint64_t)(uint8_t)(signed_val >> abs_shift);
                break;
            }
            case ND500_DTYPE_HALFWORD: {
                int16_t signed_val = (int16_t)(raw_value & 0xFFFF);
                result = (uint64_t)(uint16_t)(signed_val >> abs_shift);
                break;
            }
            case ND500_DTYPE_WORD:
            default: {
                int32_t signed_val = (int32_t)(raw_value & 0xFFFFFFFF);
                result = (uint64_t)(uint32_t)(signed_val >> abs_shift);
                break;
            }
        }
    }

    /* Mask result to data type (like C# line 77) */
    result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to operand (like C# line 78) */
    nd500_write_operand_value(cpu, &fi->operands[0], result, fi->data_type);

    /* Update status flags: Z and S (like C# line 79) */
    nd500_set_flags_zs(cpu, result, fi->data_type);
}
