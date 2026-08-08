#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * PMPY instruction - ARITHMETIC class
 *
 * PMPY - Packed Multiply
 *
 * Format: PMPY <a/r/BCD=>, <b/r/BCD=>, <c/w/BCD=>
 *
 * Assembly:
 *   PMPY (packed multiply)     Hex 0xFEB4
 *
 * Operation: <a> * <b> -> <c>
 *
 * Description:
 *   The <a> operand is multiplied by the <b> operand and the result
 *   is stored in <c>, scaled to the <c> descriptor. This instruction
 *   operates on packed BCD (Binary Coded Decimal) data.
 *   Special case: invalid digit x 0 gives 0 (not IVO).
 *
 * Trap conditions:
 *   - Addressing traps
 *   - BCD overflow (BO)
 *   - Invalid operation (IVO)
 *
 * Data status bits:
 *   - product = 0 -> Z
 *   - product.signbit -> S
 *   - BCD overflow -> BO
 *   - BO or IVO -> K
 *
 * Reference: ND-500 Reference Manual, Chapter 17.4 (Packed Arithmetic)
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Pmpy.cs
 */
void nd500_instr_Pmpy(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 3) {
        printf("[ERROR] PMPY at PC=0x%08X: Expected 3 operands, got %u\n",
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

    /* Special case: anything x 0 = 0 (no IVO even for invalid digits) */
    int64_t result;
    if (value_b == 0) {
        result = 0;
    } else {
        /* Perform BCD multiplication */
        result = value_a * value_b;
    }

    /* Write result to destination */
    nd500_write_packed_bcd_value(cpu, &desc_c, result);

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
