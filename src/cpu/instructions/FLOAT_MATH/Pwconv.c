/**
 * Pwconv instruction - FLOAT_MATH class
 *
 * Converts a packed BCD value to a binary word (32-bit signed integer).
 *
 * Opcodes:
 *   0xFEBC: W1 PWCONV - result to I1
 *   0xFEBD: W2 PWCONV - result to I2
 *   0xFEBE: W3 PWCONV - result to I3
 *   0xFEBF: W4 PWCONV - result to I4
 *
 * Operation:
 *   1. Load BCD descriptor from operand address
 *   2. Read packed BCD value from memory
 *   3. Apply scaling factor (truncate fractional part)
 *   4. Convert to 32-bit signed integer
 *   5. Store in target register (I1-I4)
 *
 * Flags:
 *   Z - Set if result is zero
 *   S - Set to sign of result
 *   O - Set if overflow (value outside int32 range)
 *   K - Set if overflow occurred
 *
 * Traps:
 *   IVO - Invalid BCD digit detected
 *   Descriptor Range - FW = 0
 *
 * Based on ND-500 CPU Reference Manual, Section 17.
 */

#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include "bcd_helpers.h"
#include <stdio.h>
#include <stdint.h>
#include <limits.h>

void nd500_instr_Pwconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] PWCONV expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine target register from opcode */
    uint8_t reg_num;
    switch (fi->opcode) {
        case 0xFEBC: reg_num = 1; break;
        case 0xFEBD: reg_num = 2; break;
        case 0xFEBE: reg_num = 3; break;
        case 0xFEBF: reg_num = 4; break;
        default:
            printf("[ERROR] PWCONV unknown opcode 0x%04X at PC=0x%08X\n",
                   fi->opcode, fi->address);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    /* Get descriptor address from operand */
    uint32_t desc_addr = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);

    /* Load BCD descriptor */
    Nd500BcdDescriptor desc = nd500_load_bcd_descriptor(cpu, desc_addr);

    if (!desc.is_valid || desc.field_width == 0) {
        printf("[TRAP] PWCONV at PC=0x%08X: Invalid BCD descriptor (FW=%u)\n",
               fi->address, desc.field_width);
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    /* Read packed BCD value */
    Nd500BcdResult bcd_result = nd500_read_packed_bcd(cpu, &desc);

    if (bcd_result.invalid_digit) {
        printf("[TRAP] PWCONV at PC=0x%08X: Invalid BCD digit detected\n",
               fi->address);
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    /* Apply scaling factor to get final integer value */
    bool overflow = false;
    int64_t scaled_value = nd500_bcd_apply_scaling(bcd_result.value, desc.scaling_factor, &overflow);

    /* Apply sign */
    if (bcd_result.is_negative) {
        scaled_value = -scaled_value;
    }

    /* Check for int32 overflow */
    int32_t result;
    if (scaled_value < INT32_MIN || scaled_value > INT32_MAX) {
        overflow = true;
        /* Truncate to 32 bits */
        result = (int32_t)scaled_value;
    } else {
        result = (int32_t)scaled_value;
    }

    /* Write result to target register */
    cpu->I[reg_num - 1] = (uint32_t)result;

    /* Set flags */
    /* Z flag: set if result is zero */
    if (result == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    /* S flag: set to sign of result */
    if (result < 0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* O flag: set if overflow occurred */
    if (overflow) {
        nd500_set_flag(cpu, ND500_FLAG_O);
        nd500_set_flag(cpu, ND500_FLAG_K);  /* K mirrors overflow status */
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_O);
        nd500_clear_flag(cpu, ND500_FLAG_K);
    }

    /* C flag: unaffected by PWCONV */
}
