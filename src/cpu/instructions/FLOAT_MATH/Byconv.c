#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <stdint.h>

/**
 * Byconv instruction - FLOAT_MATH class
 * 
 * Variants: 5
 * Mnemonics: byconv byconv byconv byconv byconv
 * Operands: 2
 * 
 * Opcodes:
 *   0xFD44 (byconv) - BI BYCONV (bit to byte)
 *   0xFD4F (byconv) - H BYCONV (halfword to byte)
 *   0xFD54 (byconv) - W BYCONV (word to byte)
 *   0xFD59 (byconv) - F BYCONV (float to byte)
 *   0xFD5E (byconv) - D BYCONV (double to byte)
 * 
 * Converts source operand to 8-bit signed byte (-128 to 127).
 * Traps IOV if value outside byte range.
 */
void nd500_instr_Byconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 2) {
        printf("[ERROR] BYCONV expects 2 operands, got %u at PC=0x%08X\n",
               fi->operand_count, fi->address);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Determine source type from opcode */
    int64_t source_value = 0;
    int8_t byte_result = 0;
    bool overflow = false;

    /* Read source operand based on opcode */
    if (fi->opcode == 0xFD44) {
        /* BI BYCONV: Zero extension (bit to byte) */
        uint64_t bit_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_BYTE);
        source_value = (bit_val & 1) ? 1 : 0;  /* Extract LSB */
    } else if (fi->opcode == 0xFD4F) {
        /* H BYCONV: Halfword to byte */
        uint64_t h_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_HALFWORD);
        int16_t signed_h = (int16_t)h_val;
        source_value = signed_h;
        if (signed_h < -128 || signed_h > 127) {
            overflow = true;
        }
    } else if (fi->opcode == 0xFD54) {
        /* W BYCONV: Word to byte */
        uint64_t w_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
        int32_t signed_w = (int32_t)w_val;
        source_value = signed_w;
        if (signed_w < -128 || signed_w > 127) {
            overflow = true;
        }
    } else if (fi->opcode == 0xFD59) {
        /* F BYCONV: Float to byte (truncate toward zero).
         * Read as FLOAT so a register operand comes from A1-A4, not I1-I4. */
        uint32_t float_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT);
        int32_t int_val = nd500_float_to_int32(float_bits);
        source_value = int_val;
        if (int_val < -128 || int_val > 127) {
            overflow = true;
        }
    } else if (fi->opcode == 0xFD5E) {
        /* D BYCONV: Double to byte (truncate toward zero) */
        uint64_t double_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        int64_t int_val = nd500_double_to_int64(double_bits);
        source_value = int_val;
        if (int_val < -128 || int_val > 127) {
            overflow = true;
        }
    } else {
        printf("[ERROR] BYCONV at PC=0x%08X: Unknown opcode 0x%04X\n",
               fi->address, fi->opcode);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Convert to byte (truncate) */
    byte_result = (int8_t)source_value;

    /* Integer overflow: the truncated result IS written (manual 15.2 -
     * "conversion of longer to shorter data types is by truncation and may
     * cause integer overflow"), then the IOV trap is raised. Flags are not
     * updated on the trap path. */
    if (overflow) {
        printf("[TRAP] BYCONV at PC=0x%08X: Value %lld outside byte range (-128 to 127)\n",
               fi->address, (long long)source_value);
        nd500_write_operand_value(cpu, &fi->operands[1], (uint64_t)(uint8_t)byte_result, ND500_DTYPE_BYTE);
        raise_trap(cpu, TRAP_IOV, fi->address, 0);
        return;
    }

    /* Write result to destination operand */
    nd500_write_operand_value(cpu, &fi->operands[1], (uint64_t)(uint8_t)byte_result, ND500_DTYPE_BYTE);

    /* Set flags: Z (zero), S (sign), O (overflow) */
    if (byte_result == 0) {
        nd500_set_flag(cpu, ND500_FLAG_Z);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_Z);
    }

    if (byte_result < 0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* O flag is set on overflow (already trapped above) */
    nd500_clear_flag(cpu, ND500_FLAG_O);

    /* C flag unaffected */
}
