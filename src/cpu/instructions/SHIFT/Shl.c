#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Shl instruction - SHIFT class
 *
 * Shift Left (bidirectional). Positive shift count shifts left, negative shifts right.
 *
 * Variants: 3 (by data type)
 * Mnemonics: SHL:H, SHL:W, SHL:D
 * Operands: 2 (value to shift, shift count)
 *
 * Opcodes:
 *   0xFCA8 (SHL:H) - Halfword shift
 *   0xFCA9 (SHL:W) - Word shift
 *   0xFCAA (SHL:D) - Doubleword shift
 *
 * Operation:
 *   If shift_count >= 0: operand << shift_count → operand
 *   If shift_count < 0:  operand >> abs(shift_count) → operand
 *
 * Description:
 *   Shifts operand left (positive count) or right (negative count).
 *   Bits shifted out are lost. Bits shifted in are zeros.
 *   Shift count is a signed byte (-127 to +127).
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *
 * Trap conditions:
 *   - IOV (Illegal Operand Value) if abs(shift_count) >= data_width_in_bits
 *
 * Reference: ND-500 Reference Manual, Chapter 10.24
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SHIFT/Shl.cs
 */
void nd500_instr_Shl(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] SHL at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operands (like C# lines 19-20) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    int8_t raw_shift = (int8_t)nd500_read_operand_byte(cpu, &fi->operands[1]); /* Signed byte */

    /* Calculate bit width from data type (like C# line 21) */
    uint32_t bits;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:      bits = 8; break;
        case ND500_DTYPE_HALFWORD:  bits = 16; break;
        case ND500_DTYPE_WORD:      bits = 32; break;
        case ND500_DTYPE_DOUBLEWORD: bits = 64; break;
        default:                    bits = 32; break;
    }

    /* Get absolute shift count (like C# line 23) */
    int32_t abs_shift = (raw_shift >= 0) ? raw_shift : -raw_shift;

    /* Validate shift count - IOV is an IGNORABLE trap per ND-500 Reference Manual 6.5.3.1
     * "If the IOV trap condition is ignored the instruction will be terminated (act as a NOOP)"
     * "On the IOV trap condition the destination field is not changed" */
    if (abs_shift >= (int32_t)bits) {
        printf("[IOV] SHL at PC=0x%08X: Shift count %d >= %u bits (treating as NOOP)\n",
               fi->address, abs_shift, bits);
        /* Don't modify destination, just return (act as NOOP) */
        return;
    }

    /* Perform shift (like C# line 30) */
    uint64_t result;
    if (raw_shift >= 0) {
        result = value << abs_shift;  /* Shift left */
    } else {
        result = value >> abs_shift;  /* Shift right (logical) */
    }

    /* Mask to data type (like C# line 31) */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to operand (like C# line 32) */
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    /* Update status flags: Z and S (like C# line 33) */
    nd500_set_flags_zs(cpu, masked_result, fi->data_type);
}
