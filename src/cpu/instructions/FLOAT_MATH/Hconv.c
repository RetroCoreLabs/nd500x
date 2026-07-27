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
        /* W HCONV (0xFD55 per ND-500 Ref Manual 15.2): WORD to halfword convert.
         * Source is a full 32-bit WORD; truncate to signed halfword with IOV check.
         * NOTE: this was previously (incorrectly) read as a HALFWORD source. On the
         * big-endian ND-500 a halfword read of a word-granular operand slot returns
         * the MOST-significant 16 bits, so any small int came back as 0. That broke
         * every (ushort) cast compiled to W HCONV - notably resume()'s
         * _resume((ushort)pstindex, &u.u_ssave), which always received pstindex=0
         * and made swtch() resume a bogus u-area (panic: sleep on every context
         * switch). See ND500X_MMU_REDESIGN_PLAN.md. */
        uint64_t w_val = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_WORD);
        int32_t signed_w = (int32_t)w_val;
        source_value = signed_w;
        /* W HCONV TRUNCATES to the low 16 bits - it does NOT raise IOV on an
         * out-of-range word. The cross-core oracle (functional CpuND500 AND the
         * microword CpuND5000, which runs the actual microcode) both give
         * i1=0x12345678 -> i2=0x5678 and i1=0xABCD -> i2=0xABCD with no trap and
         * pc advancing normally. The earlier IOV check killed the destination
         * write (i2 stayed 0) on any value >= 0x8000. Truncation is done by the
         * (uint16_t) cast at the common write below. */
    } else if (fi->opcode == 0xFD5A) {
        /* F HCONV (0xFD5A per manual 15.2): FLOAT to halfword convert.
         * (Previously misread as word source.) Convert the 32-bit float operand to
         * an integer, then truncate to halfword with IOV check. Not on the boot
         * path; see plan doc for the full CONV-family opcode-table correction. */
        uint64_t f_bits = nd500_read_operand_value(cpu, &fi->operands[0], ND500_DTYPE_FLOAT);
        int64_t int_val = (int64_t)nd500_float_to_int32((uint32_t)f_bits);
        source_value = int_val;
        if (int_val < -32768 || int_val > 32767) {
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

    /* SIGN-FLAG WIDTH. A CONVERT saves its result status at the SOURCE operand's type
     * width, not at the (narrower) result type: the ND-5000 Microprogram Guide states that
     * "in the convert instructions, the type field specifies the type of the SOURCE operand"
     * (ND-05.022.1 p.28) and that this type "also controls the result status" (Sec 5.5, p.29).
     * For W HCONV (0xFD55, microcode HCONVW @001545) the source is a WORD, so the completion
     * status-save @003220 ("ALU,OR A,SC3 B,SC4 ST,SAVA", TYP,DR = macro datatype W) latches S
     * from bit 31 of the 32-bit ZERO-EXTENDED halfword result -> always 0 (result is
     * 0x0000_0000..0x0000_FFFF). Adjudicated MICROWORD-RIGHT by a B30 single-step (CpuND5000:
     * CS 001545->001546->003217->003220, Sgn=0 for 0xABCD and 0xFFFF8000); this core previously
     * took S from the halfword sign (bit 15) and wrongly reported S=1. Same sub-word status-width
     * shape already resolved microword-right for getbf/putbf. Narrower variants keep the halfword
     * sign; only the WORD-source variant is word-width. [W HCONV S-width fix 2026-07-27] */
    if (fi->opcode != 0xFD55 && halfword_result < 0) {
        nd500_set_flag(cpu, ND500_FLAG_S);
    } else {
        nd500_clear_flag(cpu, ND500_FLAG_S);
    }

    /* O and C flags cleared */
    nd500_clear_flag(cpu, ND500_FLAG_O);
    nd500_clear_flag(cpu, ND500_FLAG_C);
}
