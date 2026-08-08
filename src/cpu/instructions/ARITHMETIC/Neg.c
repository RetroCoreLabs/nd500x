#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>

/**
 * Neg instruction - ARITHMETIC class
 *
 * Negate register (two's complement for integers, sign flip for floats).
 * -Rn → Rn
 *
 * Variants: 5 (by data type and register)
 * Mnemonics: BYn NEG, Hn NEG, Wn NEG, Fn NEG, Dn NEG (n=1..4)
 * Operands: 0 (register-only operation)
 *
 * Opcodes:
 *   0xFE08-0xFE0B (BY1 NEG through BY4 NEG) - Byte negate
 *   0xFE0C-0xFE0F (H1 NEG through H4 NEG) - Halfword negate
 *   0x0090-0x0093 (W1 NEG through W4 NEG) - Word negate
 *   0x0094-0x0097 (F1 NEG through F4 NEG) - Float negate
 *   0x0094-0x0097 (D1 NEG through D4 NEG) - Double negate
 *
 * Operation: -Rn → Rn
 *
 * Description:
 *   The contents of the specified register are negated. An integer value
 *   is negated by taking the two's complement of its value. A floating
 *   point value is negated by inverting its sign bit. Byte and halfword
 *   negate will clear the upper part of the register.
 *
 *   Integer overflow occurs if and only if the greatest negative integer
 *   is negated. Carry is zero except when integer zero is negated.
 *
 * Flags: Z (zero), S (sign), C (carry), O (overflow)
 *   Z = 1 if result is zero
 *   S = 1 if result sign bit is set
 *   C = 1 if negating zero (integer only)
 *   O = 1 if overflow (greatest negative integer negated)
 *
 * Trap conditions: Integer overflow (O)
 *
 * Reference: ND-500 Reference Manual, Chapter 10.12
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Neg.cs
 */
void nd500_instr_Neg(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    /* Validate operand count */
    if (fi->operand_count != 0) {
        printf("[ERROR] NEG at PC=0x%08X: Expected 0 operands, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    uint64_t value, result;
    bool overflow = false;
    bool carry = false;

    /* Read current register value (like C# lines 50-52) */
    if (fi->uses_float_registers) {
        if (fi->data_type == ND500_DTYPE_FLOAT || fi->data_type == ND500_DTYPE_WORD) { /* Float (F) */
            value = nd500_read_float_register(cpu, fi->target_register);
        } else { /* Double (D) */
            value = nd500_read_double_register(cpu, fi->target_register);
        }
    } else {
        value = nd500_read_integer_register(cpu, fi->target_register);
    }

    /* Perform negation (like C# lines 58-91) */
    if (fi->uses_float_registers) {
        /* Float: just invert sign bit (like C# lines 60-70) */
        /* NEG of RAW-ZERO float/double yields +0.0, NOT -0.0. ND-5000 microcode NEG_F @003244 is a
         * conditional ALU (C,ALU ALU,FZRO ALUF,XOR ... COND,MZRO): a zero operand is FORCED to +0.0
         * (FZRO); only non-zero operands take the XOR sign-flip. Keeps all 3 cores in sync
         * (microword + C# functional Neg.cs + this). [NEG zero +0.0 2026-07-25] */
        if (fi->data_type == ND500_DTYPE_FLOAT || fi->data_type == ND500_DTYPE_WORD) { /* Float (F) */
            result = (value == 0) ? 0u : (value ^ 0x80000000);
            nd500_write_float_register(cpu, fi->target_register, (uint32_t)result);
        } else { /* Double (D) */
            result = (value == 0) ? 0ull : (value ^ 0x8000000000000000ULL);
            nd500_write_double_register(cpu, fi->target_register, result);
        }
    } else {
        /* Integer: two's complement (like C# lines 72-91) */
        result = (~value + 1);

        /* Mask to data type and clear upper bits for BY/H (like C# line 78) */
        result = nd500_mask_to_datatype(result, fi->data_type);
        nd500_write_integer_register(cpu, fi->target_register, (uint32_t)result);

        /* Overflow: greatest negative negated (like C# lines 82-87) */
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE:
                overflow = (value == 0x80);
                break;
            case ND500_DTYPE_HALFWORD:
                overflow = (value == 0x8000);
                break;
            case ND500_DTYPE_WORD:
                overflow = (value == 0x80000000);
                break;
            default:
                overflow = false;
                break;
        }

        /* Carry: true only when negating zero (like C# line 90) */
        carry = (value == 0);
    }

    /* Update status flags: Z, S, C, O (like C# line 94) */
    nd500_set_flags_zsco(cpu, result, fi->data_type, carry, overflow);
}
