#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>

/**
 * Int instruction - SYSTEM class
 * 
 * Variants: 8
 * Mnemonics: int (F1-F4, D1-D4)
 * Operands: 1
 * 
 * Opcodes:
 *   0xFE60-0xFE63 (int) - F1-F4 INT (float integer part)
 *   0xFE64-0xFE67 (int) - D1-D4 INT (double integer part)
 * 
 * Calculates truncated integer part of float/double and loads result
 * into register in float/double format (not integer format).
 */
void nd500_instr_Int(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] INT expects 1 operand, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine if float or double from opcode */
    bool is_double = (fi->opcode >= 0xFE64 && fi->opcode <= 0xFE67);
    uint8_t reg_num = fi->target_register;
    
    if (reg_num < 1 || reg_num > 4) {
        printf("[ERROR] INT at PC=0x%08X: Invalid register %u\n",
               fi->address, reg_num);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    double argument = 0.0;
    double int_part = 0.0;
    uint64_t result_bits = 0;

    /* Read float/double operand */
    if (is_double) {
        uint64_t double_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        argument = nd500_double_to_ieee754(double_bits);
    } else {
        uint32_t float_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }
        argument = (double)nd500_float_to_ieee754(float_bits);
    }

    /* Truncate toward zero (integer part) */
    int_part = trunc(argument);

    /* Convert result back to ND-500 format */
    if (is_double) {
        result_bits = nd500_double_from_ieee754(int_part);
        nd500_write_double_register(cpu, reg_num, result_bits);
    } else {
        result_bits = nd500_float_from_ieee754((float)int_part);
        nd500_write_float_register(cpu, reg_num, (uint32_t)result_bits);
    }

    /* Set flags: Z (zero), S (sign) */
    if (is_double) {
        if (nd500_double_is_zero(result_bits)) {
            nd500_set_flag(cpu, ND500_FLAG_Z);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_Z);
        }
        if (nd500_double_is_negative(result_bits)) {
            nd500_set_flag(cpu, ND500_FLAG_S);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_S);
        }
    } else {
        uint32_t float_bits = (uint32_t)result_bits;
        if (nd500_float_is_zero(float_bits)) {
            nd500_set_flag(cpu, ND500_FLAG_Z);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_Z);
        }
        if (nd500_float_is_negative(float_bits)) {
            nd500_set_flag(cpu, ND500_FLAG_S);
        } else {
            nd500_clear_flag(cpu, ND500_FLAG_S);
        }
    }

    /* O, C flags unaffected */
}
