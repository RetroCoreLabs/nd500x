#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Shr instruction - SHIFT class
 *
 * Shift Rotate (circular shift). Positive count rotates LEFT, negative
 * rotates RIGHT - per manual 10.26 ("positive shiftcount implies left shift,
 * negative implies right") AND the microcode (SHR_PSC Q,Q*ROT @003334 /
 * SHR_NSC Q,Q/ROT @003330), which the body below already implements.
 *
 * CORRECTED 2026-07-30: this header used to claim the OPPOSITE, citing
 * "empirically verified against nd500-as". The body had already been fixed to
 * follow the manual and microcode, so the comment contradicted the code it
 * documented. RetroCore Shr.cs carried the identical stale comment and got the
 * same correction; its unit tests encoded the reversed direction too and were
 * fixed with it (SHR_ShouldRotateBits / SHR_SimpleRotation).
 *
 * Variants: 3 (by data type: BY, H, W; no BI/F/D variants exist)
 * Mnemonics: BY SHR, H SHR, W SHR
 * Operands: 2 (value to rotate, shift count)
 *
 * Opcodes (manual 10.26: 176256B-176260B):
 *   0xFCAE (BY SHR) - Byte rotate
 *   0xFCAF (H SHR)  - Halfword rotate
 *   0xFCB0 (W SHR)  - Word rotate
 *
 * Operation:
 *   If shift_count > 0:  rotate LEFT  (bits wrap around)
 *   If shift_count < 0:  rotate RIGHT (bits wrap around)
 *   If shift_count == 0: operand unchanged
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
 * Large shift counts:
 *   Counts >= operand width raise an IOV (Illegal Operand Value) trap,
 *   matching the manual and the C# reference (Shr.cs).
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

    /* Read operands (like C# Shr.cs lines 33-34) */
    uint64_t value = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
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

    /* Calculate bit width from data type (like C# line 35) */
    uint32_t bits;
    switch (fi->data_type) {
        case ND500_DTYPE_BYTE:      bits = 8; break;
        case ND500_DTYPE_HALFWORD:  bits = 16; break;
        case ND500_DTYPE_WORD:      bits = 32; break;
        case ND500_DTYPE_DOUBLEWORD: bits = 64; break;
        default:                    bits = 32; break;
    }

    /* Get absolute shift (like C# line 36) */
    int32_t shift = (shift_count >= 0) ? shift_count : -shift_count;

    /* Validate shift count - IOV (Illegal Operand Value) trap if the count
     * is out of range, matching C# Shr.cs. */
    if (shift >= (int32_t)bits) {
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    /* Perform circular rotate.
     * Microcode: positive count -> SHR_PSC branch (Q,Q*ROT @003334) = rotate LEFT;
     * negative count -> SHR_NSC branch (Q,Q/ROT @003330) = rotate RIGHT. Manual:
     * "Positive <shiftcount> implies left shift, negative implies right." The prior
     * code had the direction reversed. count == 0 leaves the operand unchanged. */
    uint64_t result;
    if (shift == 0) {
        result = value;
    } else if (shift_count > 0) {
        /* Rotate LEFT: (value << shift) | (value >> (bits - shift)) */
        result = (value << shift) | (value >> (bits - shift));
    } else {
        /* Rotate RIGHT: (value >> shift) | (value << (bits - shift)) */
        result = (value >> shift) | (value << (bits - shift));
    }

    /* Mask to data type */
    uint32_t masked_result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to operand */
    nd500_write_operand_value(cpu, &fi->operands[0], masked_result, fi->data_type);

    /* Status: Z and S from the result; K, C, O cleared (rule 4040 - only Z and S
     * are named for SHR). */
    nd500_set_flags_zs(cpu, masked_result, fi->data_type);
    nd500_clear_flag(cpu, ND500_FLAG_K);
    nd500_clear_flag(cpu, ND500_FLAG_C);
    nd500_clear_flag(cpu, ND500_FLAG_O);
}
