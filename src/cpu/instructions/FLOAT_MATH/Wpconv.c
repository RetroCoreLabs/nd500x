/*
 * Wpconv.c - ND-500 Wpconv instruction (FLOAT_MATH class)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

/**
 * Wpconv instruction - FLOAT_MATH class
 *
 * Converts a binary word (32-bit signed integer) to packed BCD format.
 *
 * Opcodes:
 *   0xFEB8: W1 WPCONV - source from I1
 *   0xFEB9: W2 WPCONV - source from I2
 *   0xFEBA: W3 WPCONV - source from I3
 *   0xFEBB: W4 WPCONV - source from I4
 *
 * Operation:
 *   1. Read value from source register (I1-I4)
 *   2. Load BCD descriptor from operand address
 *   3. Apply scaling factor (negative SC truncates LSB digits)
 *   4. Convert to packed BCD format
 *   5. Write to memory at descriptor base address
 *
 * Flags:
 *   Z - Set if result is zero
 *   S - Set to sign of result
 *   BO - Set if BCD overflow (value doesn't fit in field width)
 *   K - Set if BO occurred
 *
 * Traps:
 *   BO - BCD overflow (result requires more digits than field width)
 *   Descriptor Range - FW = 0
 *
 * Based on ND-500 CPU Reference Manual, Section 17.10.
 */

#include "cpu_protos.h"
#include "instructions_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "bcd_helpers.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void nd500_instr_Wpconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] WPCONV expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine source register from opcode */
    uint8_t reg_num;
    switch (fi->opcode) {
        case 0xFEB8: reg_num = 1; break;
        case 0xFEB9: reg_num = 2; break;
        case 0xFEBA: reg_num = 3; break;
        case 0xFEBB: reg_num = 4; break;
        default:
            printf("[ERROR] WPCONV unknown opcode 0x%04X at PC=0x%08X\n",
                   fi->opcode, fi->address);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Read source value from register */
    int32_t source_value = (int32_t)cpu->I[reg_num - 1];

    /* Get descriptor address from operand - use effective_address, NOT the value */
    uint32_t desc_addr = fi->operands[0].effective_address;

    /* Load BCD descriptor */
    Nd500BcdDescriptor desc = nd500_load_bcd_descriptor(cpu, desc_addr);

    if (!desc.is_valid || desc.field_width == 0) {
        ND500X_TRAPLOG("[TRAP] WPCONV at PC=0x%08X: Invalid BCD descriptor (FW=%u)\n",
               fi->address, desc.field_width);
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    /* Determine sign */
    bool is_negative = (source_value < 0);
    int64_t abs_value = is_negative ? -(int64_t)source_value : (int64_t)source_value;

    /* Apply scaling factor for BCD storage
     * Positive SC: value represents value * 10^SC, so we store value as-is
     *              (the decimal interpretation happens when reading)
     * Negative SC: value is divided by 10^|SC|, losing fractional part
     *              Example: 12345 with SC=-2 means we're storing 123.45,
     *              but the BCD stores the digits "12345"
     * For WPCONV: the integer value needs to be adjusted for the destination scale
     */
    bool overflow = false;
    int64_t bcd_value;

    if (desc.scaling_factor >= 0) {
        /* Positive or zero scaling: value stored directly
         * The scaling factor indicates the value is multiplied by 10^SC
         * when interpreted, so we store the base digits */
        bcd_value = abs_value;
    } else {
        /* Negative scaling: multiply by 10^|SC| to get the BCD digits
         * Example: value=123 with SC=-2 means 123.00, stored as "12300" */
        bcd_value = nd500_bcd_remove_scaling(abs_value, desc.scaling_factor, &overflow);
    }

    /* Check if value fits in field width */
    bool bcd_overflow = !nd500_bcd_value_fits((uint64_t)bcd_value, desc.field_width);

    /* Write packed BCD value to memory */
    Nd500BcdResult write_result = nd500_write_packed_bcd(cpu, &desc, bcd_value, is_negative);

    if (write_result.overflow) {
        bcd_overflow = true;
    }

    /* Set flags */
    /* Z flag: set if result is zero */
    if (source_value == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    /* S flag: set to sign of value */
    if (is_negative) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* BO flag: set if BCD overflow occurred */
    /* Note: BO is a separate flag from O (integer overflow) */
    /* For now, we'll use O flag position since BO trap uses TRAP_BO */
    if (bcd_overflow || overflow) {
        /* Set BO and K flags */
        nd500_set_flag(cpu, ND500_FLAG_O);  /* BO uses same position as O in some implementations */
        nd500_set_flag(cpu, ND500_FLAG_K);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_O);
        nd500_clear_flag(cpu, ND500_FLAG_K);
    }

    /* C flag: unaffected by WPCONV */
}
