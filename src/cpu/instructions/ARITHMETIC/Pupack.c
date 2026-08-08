#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * PUPACK instruction - ARITHMETIC class
 *
 * PUPACK - Convert Packed BCD to ASCII
 *
 * Format: PUPACK <source/r/BCD=>, <dest/w/ASCII=>
 *
 * Assembly:
 *   PUPACK (convert packed to ASCII)  Hex 0xFEB6
 *
 * Operation: <source> -> <dest>
 *
 * Description:
 *   Unpack BCD to ASCII. Sign determined by SGN in <dest> descriptor.
 *   Extend with leading ASCII zeros if necessary; parity zero for all digits.
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
 * Reference: ND-500 Reference Manual, Chapter 17.8 (Convert packed to ASCII)
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Pupack.cs
 */
void nd500_instr_Pupack(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] PUPACK at PC=0x%08X: Expected 2 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Get descriptor addresses from operands */
    uint32_t source_desc_addr = fi->operands[0].effective_address;
    uint32_t dest_desc_addr = fi->operands[1].effective_address;

    /* Load string descriptors - source is BCD, dest is ASCII */
    Nd500StringDescriptor source_desc, dest_desc;

    if (!nd500_load_string_descriptor(cpu, source_desc_addr, true, false, &source_desc)) {
        return;
    }
    if (!nd500_load_string_descriptor(cpu, dest_desc_addr, false, true, &dest_desc)) {
        return;
    }

    /* Read BCD value */
    int64_t value = nd500_read_packed_bcd_value(cpu, &source_desc);

    /* Determine if negative */
    bool is_negative = (value < 0);
    uint64_t abs_value = is_negative ? (uint64_t)(-value) : (uint64_t)value;

    /* Write as ASCII string */
    uint32_t count = dest_desc.element_count;
    uint32_t write_pos = count;

    /* Convert value to ASCII digits from right to left */
    uint8_t digits[32];
    int num_digits = 0;

    if (abs_value == 0) {
        digits[0] = '0';
        num_digits = 1;
    } else {
        while (abs_value > 0 && num_digits < 32) {
            digits[num_digits++] = '0' + (abs_value % 10);
            abs_value /= 10;
        }
    }

    /* Write digits to destination (right-aligned with leading zeros) */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t addr = dest_desc.base_address + count - 1 - i;
        char ch;
        if ((int)i < num_digits) {
            ch = digits[i];
        } else {
            ch = '0';  /* Leading zero */
        }
        nd500_write_memory_8(cpu, addr, (uint8_t)ch);
    }

    /* Handle sign if needed - write '-' at first position if negative */
    if (is_negative && count > 0) {
        nd500_write_memory_8(cpu, dest_desc.base_address, '-');
    }

    /* Update status flags */
    if (value == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (is_negative) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }
}
