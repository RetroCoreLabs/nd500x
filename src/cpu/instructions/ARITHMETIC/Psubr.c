#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * PSUBR instruction - ARITHMETIC class
 *
 * PSUBR - Packed Subtract Rounded
 *
 * Format: PSUBR <a/r/BCD=>, <b/r/BCD=>, <c/w/BCD=>
 *
 * Assembly:
 *   PSUBR (packed subtract rounded)  Hex 0xFE86
 *
 * Operation: <a> - <b> -> <c> (with rounding)
 *
 * Description:
 *   The <b> operand is subtracted from the <a> operand and the result
 *   is stored in <c>, scaled to the <c> descriptor with rounding. This
 *   instruction operates on packed BCD (Binary Coded Decimal) data.
 *
 * Trap conditions:
 *   - Addressing traps
 *   - BCD overflow (BO)
 *   - Invalid operation (IVO)
 *
 * Data status bits:
 *   - difference = 0 -> Z
 *   - difference.signbit -> S
 *   - BCD overflow -> BO
 *   - BO or IVO -> K
 *
 * Reference: ND-500 Reference Manual, Chapter 17.3 (Packed Arithmetic)
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Psubr.cs
 */
void nd500_instr_Psubr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 3) {
        printf("[ERROR] PSUBR at PC=0x%08X: Expected 3 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses from operands */
    uint32_t desc_addr_a = fi->operands[0].effective_address;
    uint32_t desc_addr_b = fi->operands[1].effective_address;
    uint32_t desc_addr_c = fi->operands[2].effective_address;

    /* Load string descriptors for BCD packed operands */
    Nd500StringDescriptor desc_a, desc_b, desc_c;

    if (!nd500_load_string_descriptor(cpu, desc_addr_a, true, false, &desc_a)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, desc_addr_b, true, false, &desc_b)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, desc_addr_c, true, false, &desc_c)) {
        return;
    }

    /* Read BCD values */
    int64_t value_a = nd500_read_packed_bcd_value(cpu, &desc_a);
    int64_t value_b = nd500_read_packed_bcd_value(cpu, &desc_b);

    /* Perform BCD subtraction */
    int64_t result = value_a - value_b;

    /* Determine source scale (use maximum precision of operands for subtraction) */
    int8_t source_scale = desc_a.scaling_factor;
    if (desc_b.scaling_factor > source_scale) {
        source_scale = desc_b.scaling_factor;
    }

    /* Write result to destination WITH ROUNDING */
    nd500_write_packed_bcd_value_rounded(cpu, &desc_c, result, source_scale);

    /* Update status flags */
    if (result == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (result < 0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }
}
