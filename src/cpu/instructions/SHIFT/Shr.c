#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Shr instruction - SHIFT class
 *
 * Shift Rotate (circular shift). Positive rotates left, negative rotates right.
 *
 * Variants: 3 (by data type)
 * Mnemonics: SHR:H, SHR:W, SHR:D
 * Operands: 2 (value to rotate, shift count)
 *
 * Opcodes:
 *   0xFCAE (SHR:H) - Halfword rotate
 *   0xFCAF (SHR:W) - Word rotate
 *   0xFCB0 (SHR:D) - Doubleword rotate
 *
 * Operation:
 *   If shift_count >= 0: rotate left (bits wrap around)
 *   If shift_count < 0:  rotate right (bits wrap around)
 *
 * Description:
 *   Circular shift (rotate) operand. Bits shifted out on one end
 *   wrap around to the other end. No bits are lost.
 *   Shift count is a signed byte (-127 to +127).
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *
 * Trap conditions:
 *   - IOV (Illegal Operand Value) if abs(shift_count) >= data_width_in_bits
 *
 * Reference: ND-500 Reference Manual, Chapter 10.26
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SHIFT/Shr.cs
 */
void nd500_instr_Shr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] SHR at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands (like C# lines 19-20) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    int8_t shift_count = (int8_t)nd500_read_operand_byte(cpu, &fi->operands[1]); /* Signed byte */

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
    int32_t shift = (shift_count >= 0) ? shift_count : -shift_count;

    /* Validate shift count (like C# lines 23-27) */
    if (shift >= (int32_t)bits) {
        printf("[TRAP] SHR at PC=0x%08X: Illegal shift count %d (>= %u bits)\n",
               fi->address, shift, bits);
        trap_invalid_operation(cpu, fi->address);  /* IOV trap */
        return;
    }

    /* Perform circular shift (like C# line 28) */
    uint64_t result;
    if (shift_count >= 0) {
        /* Rotate left: (value << shift) | (value >> (bits - shift)) */
        result = (value << shift) | (value >> (bits - shift));
    } else {
        /* Rotate right: (value >> shift) | (value << (bits - shift)) */
        result = (value >> shift) | (value << (bits - shift));
    }

    /* Mask to data type (like C# line 29) */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to operand (like C# line 30) */
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    /* Update status flags: Z and S (like C# line 31) */
    nd500_set_flags_zs(cpu, masked_result, fi->data_type);
}
