#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdint.h>

/**
 * Hconv instruction - FLOAT_MATH class
 *
 * Variants: 5
 * Mnemonics: hconv hconv hconv hconv hconv
 * Operands: 2
 *
 * Opcodes:
 *   0xFD45 (hconv) - BI HCONV (bit to halfword)
 *   0xFD4A (hconv) - BY HCONV (byte to halfword)
 *   0xFD55 (hconv) - H HCONV (halfword to halfword - no-op)
 *   0xFD5A (hconv) - W HCONV (word to halfword)
 *   0xFD5F (hconv) - D HCONV (double to halfword)
 *
 * Converts source operand to 16-bit signed halfword (-32768 to 32767).
 * Traps IOV if value outside halfword range.
 */
void nd500_instr_Hconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] HCONV expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine source type from opcode */
    int64_t source_value = 0;
    int16_t halfword_result = 0;
    bool overflow = false;

    /* Read source operand based on opcode */
    if (fi->opcode == 0xFD45) {
        /* BI HCONV: Zero extension (bit to halfword) */
        uint64_t bit_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
        source_value = (bit_val & 1) ? 1 : 0;  /* Extract LSB */
    } else if (fi->opcode == 0xFD4A) {
        /* BY HCONV: Byte to halfword (sign extension) */
        uint64_t byte_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
        int8_t signed_byte = (int8_t)byte_val;
        source_value = signed_byte;  /* Sign extend */
    } else if (fi->opcode == 0xFD55) {
        /* H HCONV: Halfword to halfword (direct copy) */
        uint64_t h_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_HALFWORD);
        source_value = (int16_t)h_val;
    } else if (fi->opcode == 0xFD5A) {
        /* W HCONV: Word to halfword (truncate, check overflow) */
        uint64_t w_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
        int32_t signed_w = (int32_t)w_val;
        source_value = signed_w;
        if (signed_w < -32768 || signed_w > 32767) {
            overflow = true;
        }
    } else if (fi->opcode == 0xFD5F) {
        /* D HCONV: Double to halfword (truncate toward zero) */
        uint64_t double_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        int64_t int_val = nd500_double_to_int64(double_bits);
        source_value = int_val;
        if (int_val < -32768 || int_val > 32767) {
            overflow = true;
        }
    } else {
        printf("[ERROR] HCONV at PC=0x%08X: Unknown opcode 0x%04X\n",
               fi->address, fi->opcode);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Check for overflow trap */
    if (overflow) {
        printf("[TRAP] HCONV at PC=0x%08X: Value %lld outside halfword range (-32768 to 32767)\n",
               fi->address, (long long)source_value);
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    /* Convert to halfword (truncate) */
    halfword_result = (int16_t)source_value;

    /* Write result to destination operand */
    nd500_write_operand_value(cpu, &fi->operands[1], (uint64_t)(uint16_t)halfword_result, ND500_DTYPE_HALFWORD);

    /* Set flags: Z (zero), S (sign) */
    if (halfword_result == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (halfword_result < 0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* O and C flags cleared */
    nd500_clear_flag(cpu, ND500_FLAG_O);
    nd500_clear_flag(cpu, ND500_FLAG_C);
}
