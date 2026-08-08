#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Inv instruction - LOGICAL class
 *
 * Invert register (one's complement). Rn = ~Rn
 *
 * Variants: 4 (by data type and register)
 * Mnemonics: BIn INV, BYn INV, Hn INV, Wn INV (n=1..4)
 * Operands: 0 (register-only operation)
 *
 * Opcodes:
 *   0xFE10-0xFE13 (BI1 INV through BI4 INV) - Bit invert
 *   0xFE14-0xFE17 (BY1 INV through BY4 INV) - Byte invert
 *   0xFE18-0xFE1B (H1 INV through H4 INV) - Halfword invert
 *   0x0098-0x009B (W1 INV through W4 INV) - Word invert
 *
 * Operation: Rn ← ~Rn (one's complement)
 *
 * Description:
 *   The one's complement of the contents of the specified register is
 *   calculated and stored in the same register. When the datatype is BI,
 *   BY, or H only the lower part of the register is complemented and
 *   the rest of the register is cleared.
 *
 * Flags: Z (zero), S (sign)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *
 * Trap conditions: None
 *
 * Reference: ND-500 Reference Manual, Chapter 10.13
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/LOGICAL/Inv.cs
 */
void nd500_instr_Inv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 0) {
        printf("[ERROR] INV at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Read register value (like C# line 44) */
    uint32_t value = nd500_read_integer_register(cpu, fi->target_register);

    /* One's complement (like C# line 47) */
    uint32_t result = ~value;

    /* Mask to data type - clears upper bits for BI, BY, H (like C# line 50) */
    result = nd500_mask_to_datatype(result, fi->data_type);

    /* Write back to register (like C# line 53) */
    nd500_write_integer_register(cpu, fi->target_register, result);

    /* Update status flags: Z and S (like C# line 56) */
    nd500_set_flags_zs(cpu, result, fi->data_type);
}
