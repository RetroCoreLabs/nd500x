#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * PPACK instruction - ARITHMETIC class
 *
 * PPACK - Convert ASCII to Packed BCD
 *
 * Format: PPACK <source/r/ASCII=>, <dest/w/BCD=>
 *
 * Assembly:
 *   PPACK (convert ASCII to packed)  Hex 0xFEB5
 *
 * Operation: <source> -> <dest>
 *
 * Description:
 *   Pack ASCII coded decimal to BCD. Unsigned if <dest> bit 26 set;
 *   otherwise sign from <source>. This instruction converts ASCII
 *   decimal numbers to packed BCD format for arithmetic operations.
 *
 * Trap conditions:
 *   - Addressing traps
 *   - BCD overflow (BO)
 *   - Invalid operation (IVO)
 *
 * Data status bits:
 *   - value after rounding = 0 -> Z
 *   - value.signbit -> S
 *   - BCD overflow -> BO
 *   - BO or IVO -> K
 *
 * Reference: ND-500 Reference Manual, Chapter 17.7 (Convert ASCII to packed)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Ppack.cs
 */
void nd500_instr_Ppack(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] PPACK at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses from operands */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t dest_desc_addr = fi->operands[1].effective_address;

    /* Load string descriptors - source is ASCII, dest is BCD */
    Nd500StringDescriptor source_desc, dest_desc;

    if (!nd500_load_string_descriptor(cpu, source_desc_addr, false, true, &source_desc)) {
        return;  /* Invalid descriptor */
    }
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, true, false, &dest_desc)) {
        return;
    }

    /* Read ASCII value by parsing ASCII digits */
    int64_t value = 0;
    bool is_negative = false;
    uint32_t count = source_desc.element_count;

    for (uint32_t i = 0; i < count; i++) {
        uint32_t addr = source_desc.base_address + i;
        uint8_t ch = nd500_read_memory_8(cpu, addr);

        /* Handle sign characters */
        if (ch == '+') {
            is_negative = false;
            continue;
        } else if (ch == '-') {
            is_negative = true;
            continue;
        }

        /* Handle digit characters '0'-'9' */
        if (ch >= '0' && ch <= '9') {
            value = value * 10 + (ch - '0');
        }
        /* Non-digit characters are ignored */
    }

    if (is_negative) {
        value = -value;
    }

    /* Write as BCD value */
    nd500_write_packed_bcd_value(cpu, &dest_desc, value);

    /* Update status flags */
    if (value == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (value < 0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }
}
