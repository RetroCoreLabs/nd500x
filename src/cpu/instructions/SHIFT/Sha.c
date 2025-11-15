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

    /* Read operands - value is treated as SIGNED (like C# line 19) */
    int64_t value = (int64_t)nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    int8_t shift_count = (int8_t)nd500_read_operand_byte(cpu, &fi->operands[1]); /* Signed byte */

    /* Sign-extend value based on data type */
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:
            value = (int8_t)value;
            break;
        case ND500_DTYPE_HALFWORD:
            value = (int16_t)value;
            break;
        case ND500_DTYPE_WORD:
            value = (int32_t)value;
            break;
        default:
            /* DOUBLEWORD already 64-bit */
            break;
    }

    /* Calculate bit width from data type (like C# line 21) */
    uint32_t bits;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:      bits = 8; break;
        case ND500_DTYPE_HALFWORD:  bits = 16; break;
        case ND500_DTYPE_WORD:      bits = 32; break;
        case ND500_DTYPE_DOUBLEWORD: bits = 64; break;
        default:                    bits = 32; break;
    }

    /* Get absolute shift (like C# line 22) */
    int32_t abs_shift = (shift_count >= 0) ? shift_count : -shift_count;

    /* Validate shift count (like C# lines 23-27) */
    if (abs_shift >= (int32_t)bits) {
        printf("[TRAP] SHA at PC=0x%08X: Illegal shift count %d (>= %u bits)\n",
               fi->address, abs_shift, bits);
        trap_invalid_operation(cpu, fi->address);  /* IOV trap */
        return;
    }

    /* Perform arithmetic shift (like C# line 28) */
    int64_t result;
    if (shift_count >= 0) {
        result = value << abs_shift;  /* Shift left */
    } else {
        result = value >> abs_shift;  /* Arithmetic right shift (sign extends) */
    }

    /* Write back to operand (like C# line 29) */
    nd500_write_operand_value(cpu, &fi->operands[0], (uint64_t)result, fi->data_type);

    /* Update status flags: Z and S (like C# line 30) */
    nd500_set_flags_zs(cpu, (uint64_t)result, fi->data_type);
}
