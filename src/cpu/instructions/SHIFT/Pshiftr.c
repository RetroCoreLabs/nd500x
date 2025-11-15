#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Pshiftr instruction - SHIFT class
 *
 * Packed Shift Right. Shifts operand right by shift count.
 *
 * Variants: 9
 * Mnemonics: PSHIFTR
 * Operands: 2 (operand to shift, shift count)
 *
 * Opcodes:
 *   0xFE87-0xFFF5 (9 variants)
 *
 * Operation: operand >> shift_count → operand
 *
 * Description:
 *   Performs a logical right shift operation. The shift count is a signed
 *   byte. For this basic implementation, it operates as a standard logical
 *   right shift. "Packed" semantics (parallel shift of multiple sub-fields)
 *   may be refined later based on actual usage patterns.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *
 * Trap conditions:
 *   - IOV (Illegal Operand Value) if abs(shift_count) >= data_width_in_bits
 *
 * Reference: ND-500 Reference Manual, Shift Operations
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/SHIFT/Pshiftr.cs
 *
 * IMPLEMENTATION STATUS: BASIC - Implements simple logical right shift.
 * Packed/parallel semantics may need refinement based on usage patterns.
 */
void nd500_instr_Pshiftr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] PSHIFTR at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read operand value and shift count (like C# lines 58-59) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    int8_t shift_count = (int8_t)nd500_read_operand_byte(cpu, &fi->operands[1]); /* Signed byte */

    /* Calculate bit width from data type (like C# line 62) */
    uint32_t bits;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:      bits = 8; break;
        case ND500_DTYPE_HALFWORD:  bits = 16; break;
        case ND500_DTYPE_WORD:      bits = 32; break;
        case ND500_DTYPE_DOUBLEWORD: bits = 64; break;
        default:                    bits = 32; break;
    }

    /* Get absolute shift count (like C# line 63) */
    int32_t abs_shift = (shift_count >= 0) ? shift_count : -shift_count;

    /* Validate shift count (like C# lines 66-70) */
    if (abs_shift >= (int32_t)bits) {
        printf("[TRAP] PSHIFTR at PC=0x%08X: Illegal shift count %d (>= %u bits)\n",
               fi->address, abs_shift, bits);
        trap_invalid_operation(cpu, fi->address);  /* IOV trap */
        return;
    }

    /* Perform right shift (like C# line 75) */
    /* For now, implement as standard logical right shift */
    /* NOTE: "Packed" shift may have special semantics for operating on
     * multiple sub-fields within a word. Current implementation treats
     * it as a standard logical right shift (matching C# implementation).
     * Refinement may be needed based on actual program usage patterns. */
    uint64_t result = value >> abs_shift;

    /* Mask to data type (like C# line 78) */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to operand (like C# line 81) */
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    /* Update status flags: Z and S (like C# line 84) */
    nd500_set_flags_zs(cpu, masked_result, fi->data_type);
}
