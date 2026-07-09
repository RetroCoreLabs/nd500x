#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdint.h>

/**
 * Dconv instruction - FLOAT_MATH class
 * 
 * Variants: 5
 * Mnemonics: dconv dconv dconv dconv dconv
 * Operands: 2
 * 
 * Opcodes:
 *   0xFD48 (dconv) - BI DCONV (bit to double)
 *   0xFD4D (dconv) - BY DCONV (byte to double)
 *   0xFD52 (dconv) - H DCONV (halfword to double)
 *   0xFD57 (dconv) - W DCONV (word to double)
 *   0xFD5C (dconv) - F DCONV (float to double)
 * 
 * Converts source operand to 64-bit ND-500 double precision float.
 */
void nd500_instr_Dconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] DCONV expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint64_t double_result = 0;

    /* Read source operand based on opcode */
    if (fi->opcode == 0xFD48) {
        /* BI DCONV: Bit to double */
        uint64_t bit_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
        int64_t int_val = (bit_val & 1) ? 1 : 0;
        double_result = nd500_double_from_int64(int_val);
    } else if (fi->opcode == 0xFD4D) {
        /* BY DCONV: Byte to double */
        uint64_t byte_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
        int64_t int_val = (int64_t)(int8_t)byte_val;  /* Sign extend */
        double_result = nd500_double_from_int64(int_val);
    } else if (fi->opcode == 0xFD52) {
        /* H DCONV: Halfword to double */
        uint64_t h_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_HALFWORD);
        int64_t int_val = (int64_t)(int16_t)h_val;  /* Sign extend */
        double_result = nd500_double_from_int64(int_val);
    } else if (fi->opcode == 0xFD57) {
        /* W DCONV: Word to double */
        uint64_t w_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
        int64_t int_val = (int64_t)(int32_t)w_val;  /* Sign extend */
        double_result = nd500_double_from_int64(int_val);
    } else if (fi->opcode == 0xFD5C) {
        /* F DCONV: Float to double.
         * Read as FLOAT so a register operand comes from A1-A4, not I1-I4. */
        uint32_t float_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT);
        double_result = nd500_single_to_double(float_bits);
    } else {
        printf("[ERROR] DCONV at PC=0x%08X: Unknown opcode 0x%04X\n",
               fi->address, fi->opcode);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Write result to destination operand */
    nd500_write_operand_value(cpu, &fi->operands[1], double_result, ND500_DTYPE_DOUBLEWORD);

    /* Set flags: Z (zero), S (sign) */
    if (nd500_double_is_zero(double_result)) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (nd500_double_is_negative(double_result)) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* O and C flags unaffected */
}
