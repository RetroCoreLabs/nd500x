#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdbool.h>

/**
 * PUPACKR instruction - ARITHMETIC class
 *
 * PUPACKR - Convert Packed BCD to ASCII Rounded
 *
 * Format: PUPACKR <source/r/BCD=>, <dest/w/ASCII=>
 *
 * Assembly:
 *   PUPACKR (convert packed to ASCII rounded)  Hex 0xFE93
 *
 * Operation: <source> -> <dest> (with rounding)
 *
 * Description:
 *   Unpack BCD to ASCII with rounding. Sign determined by SGN in <dest>
 *   descriptor. Extend with leading ASCII zeros if necessary.
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
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Pupackr.cs
 */
void nd500_instr_Pupackr(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 2) {
        printf("[ERROR] PUPACKR at PC=0x%08X: Expected 2 operands, got %u\n",
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

    /* Apply rounding based on source BCD scale vs destination ASCII scale */
    /* Destination ASCII scale from descriptor determines output precision */
    int scale_diff = source_desc.scaling_factor - dest_desc.scaling_factor;
    if (scale_diff > 0) {
        /* Round when reducing precision */
        int64_t divisor = 1;
        for (int i = 0; i < scale_diff; i++) {
            divisor *= 10;
        }
        int64_t half = divisor / 2;
        if (value >= 0) {
            value = (value + half) / divisor;
        } else {
            value = (value - half) / divisor;
        }
    } else if (scale_diff < 0) {
        /* Increase precision by multiplying */
        int64_t multiplier = 1;
        for (int i = 0; i < -scale_diff; i++) {
            multiplier *= 10;
        }
        value *= multiplier;
    }

    /* Determine if negative */
    bool is_negative = (value < 0);
    uint64_t abs_value = is_negative ? (uint64_t)(-value) : (uint64_t)value;

    /* Write as ASCII string */
    uint32_t count = dest_desc.element_count;

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

    /* Handle sign if needed */
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
