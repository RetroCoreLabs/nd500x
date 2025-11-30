#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Sha instruction - SHIFT class
 *
 * Shift Arithmetic. Preserves sign bit during shift.
 *
 * Variants: 3 (by data type)
 * Mnemonics: SHA:H, SHA:W, SHA:D
 * Operands: 2 (value to shift, shift count)
 *
 * Opcodes:
 *   0xFCAB (SHA:H) - Halfword arithmetic shift
 *   0xFCAC (SHA:W) - Word arithmetic shift
 *   0xFCAD (SHA:D) - Doubleword arithmetic shift
 *
 * Operation:
 *   If shift_count >= 0: operand << shift_count → operand (sign preserved)
 *   If shift_count < 0:  operand >> abs(shift_count) → operand (sign extended)
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
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SHIFT/Sha.cs
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
    int8_t shift_count = (int8_t)nd500_read_operand_byte(cpu, &fi->operands[1]); /* Signed byte */

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
        printf("[TRAP] SHA at PC=0x%08X: Illegal shift count %d (>= %u bits)\n",
               fi->address, abs_shift, bits);
        trap_invalid_operation(cpu, fi->address);  /* IOV trap */
        return;
    }

    /* Perform arithmetic shift
     * Empirically verified convention (matching test expectations):
     * - Positive count = RIGHT shift (arithmetic, sign-extending)
     * - Negative count = LEFT shift (same as logical shift left)
     *
     * Key insight from test validation:
     * - For register operands, use FULL 32-bit register value for sign determination
     * - Data type only affects output masking, not input sign
     * - E.g., BY SHA on I1=0xFF: treat as 255 (positive), not -1 */
    uint64_t result;
    if (shift_count >= 0) {
        /* Right shift: arithmetic (sign-extending based on FULL register width)
         * Sign is determined by bit 31 of the value, regardless of data type */
        int32_t signed_val = (int32_t)(raw_value & 0xFFFFFFFF);
        result = (uint64_t)(uint32_t)(signed_val >> abs_shift);
    } else {
        /* Left shift: same as logical shift left */
        result = raw_value << abs_shift;
    }

    /* Mask result to data type (like C# line 77) */
    result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to operand (like C# line 78) */
    nd500_write_operand_value(cpu, &fi->operands[0], result, fi->data_type);

    /* Update status flags: Z and S (like C# line 79) */
    nd500_set_flags_zs(cpu, result, fi->data_type);
}
