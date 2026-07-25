#include "cpu_protos.h"
#include "machine_protos.h"
#include "instruction_helpers.h"
#include <stdio.h>
#include <math.h>

/**
 * Divide instruction - ARITHMETIC class
 *
 * Divide Register by Operand: Rn / <operand> → Rn
 *
 * Variants: 5
 * Mnemonics: / (divide)
 * Operands: 1 (<operand/r/t>)
 *
 * Opcodes:
 *   0xFC4C (BYn /) byte divide
 *   0xFC50 (Hn /)  halfword divide
 *   0x0078 (Wn /)  word divide
 *   0x007C (Fn /)  float divide
 *   0x0080 (Dn /)  double divide
 *
 * Operation: Rn / <operand> → Rn
 *
 * Description:
 *   The contents of the specified register are divided by the operand.
 *   The quotient is stored in the register. The remainder is discarded.
 *   Division by zero causes a divide by zero trap.
 *
 *   Register selection is encoded in opcode bits 1-0:
 *   - 00 → register 1 (I1, A1)
 *   - 01 → register 2 (I2, A2)
 *   - 10 → register 3 (I3, A3)
 *   - 11 → register 4 (I4, A4)
 *
 *   For integer variants: Uses I1-I4 registers
 *   For float/double: Uses A1-A4 (float) or D1-D4 (A+E pairs, double)
 *
 * Flags: Z (zero), S (sign), O (overflow), DZ (divide by zero)
 *   Z = 1 if quotient is zero
 *   S = 1 if sign bit is set
 *   O = 1 if overflow (MIN_VALUE / -1)
 *   DZ = 1 if divisor is zero
 *
 * Trap conditions:
 *   - Addressing traps
 *   - Divide by zero (DZ)
 *   - Integer overflow (O)
 *   - Floating overflow (FO)
 *   - Floating underflow (FU)
 *
 * Reference: ND-500 Reference Manual, Chapter 11 (Basic Arithmetic)
 *            RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Divide.cs
 */
void nd500_instr_Divide(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    if (fi->operand_count != 1) {
        printf("[ERROR] Divide at PC=0x%08X: Expected 1 operand, got %u\n",
               fi->address, fi->operand_count);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    if (fi->target_register < 1 || fi->target_register > 4) {
        printf("[ERROR] Divide at PC=0x%08X: Invalid target register %u\n",
               fi->address, fi->target_register);
        trap_illegal_operand(cpu, fi->address);
        return;
    }

    /* Handle float/double variants */
    if (fi->uses_float_registers) {
        bool is_double = (fi->data_type == ND500_DTYPE_DOUBLEWORD);
        uint8_t reg_num = fi->target_register;

        if (!is_double) {
            /* ---------------------------------------------------------------
             * SINGLE-precision float divide (DIVF, microcode @002501 ->
             * DIVFI_00 @023211 -> QUOT_RND/QUOT_EXP @023276 -> QUOT_FIN_2).
             *
             * This is NOT a host `double` division: the ND-500 divide is a
             * bit-level reciprocal/remainder long-division and rounds the
             * quotient by ROUNDING THE MAGNITUDE UP whenever the remainder is
             * nonzero (round-away-from-zero on any inexactness), not IEEE
             * round-to-nearest. Doing the host-double divide + narrowing cast
             * lands 1 ULP off on the inexact cases (e.g. 0.1 -> 0x3F666667).
             * The exact-integer model below was validated bit-for-bit AND
             * flag-for-flag against all 32 cross-core FloatDiv oracle cases.
             *
             * ND-500 single format: sign bit31 | 9-bit exp (bias 256, bits
             * 30-22) | 22-bit mantissa (bits 21-0), value =
             *   +/- (2^22 + mant)/2^23 * 2^(efield - 256), with an implicit
             * leading 1 at bit 22 (significand in [0.5, 1)). efield==0 with a
             * zero mantissa is the only encoding of exactly 0.0.
             * ------------------------------------------------------------- */
            uint32_t nb = nd500_read_float_register(cpu, reg_num);
            uint32_t db = (uint32_t)nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

            uint32_t en = (nb >> 22) & 0x1FF, mn = nb & 0x3FFFFF;
            uint32_t ed = (db >> 22) & 0x1FF, md = db & 0x3FFFFF;
            bool n_zero = (en == 0 && mn == 0);
            bool d_zero = (ed == 0 && md == 0);

            /* Divide-by-zero: ONLY when the divisor bits are exactly zero. A
             * "dirty zero" (efield==0, nonzero mantissa) is a real tiny number
             * and divides normally (may overflow). Microcode DIVFI_DZ path ->
             * DIVFI_ST loads ST with DZ; the register is left unchanged. */
            if (d_zero) {
                cpu->ST1 |= ND500_FLAG_DZ;
                trap_divide_by_zero(cpu, fi->address);
                return;
            }

            /* 0.0 / x = 0.0. A normal quotient sets ONLY the S (sign) flag from
             * the result sign - the cross-core oracle shows Z is NOT set even
             * when the quotient is zero (e.g. F2 / b.0: 0.0/x -> a1=0, st=0),
             * and positive results give st=0 while negative give st=0x80.
             * Clear the FP arithmetic status bits and, for a +0 result, leave
             * S clear. */
            if (n_zero) {
                nd500_write_float_register(cpu, reg_num, 0);
                cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O |
                              ND500_FLAG_DZ | ND500_FLAG_FO | ND500_FLAG_FU);
                return;
            }

            uint32_t sign = ((nb >> 31) & 1) ^ ((db >> 31) & 1);
            uint64_t sig_n = (1u << 22) | mn;   /* 23-bit significand [2^22,2^23) */
            uint64_t sig_d = (1u << 22) | md;

            /* Long-divide the significands with 24 guard bits. Q holds
             * ratio_sig * 2^24 (ratio_sig in (0.5,2) -> Q in (2^23, 2^25));
             * a nonzero remainder means the quotient is inexact. */
            uint64_t num = sig_n << 24;
            uint64_t Q = num / sig_d;
            uint64_t rem = num % sig_d;

            /* Normalise Q down to a 23-bit significand [2^22, 2^23). */
            int topbit = 63 - __builtin_clzll(Q);
            int shift = topbit - 22;                 /* always >= 1 here */
            uint64_t sq = Q >> shift;
            bool lost = ((Q & (((uint64_t)1 << shift) - 1)) != 0) || (rem != 0);

            /* Round the MAGNITUDE up on any inexactness (round-away-from-zero),
             * matching QUOT_RND's remainder-based rounding. */
            if (lost) {
                sq++;
                if (sq == (1u << 23)) { sq >>= 1; shift++; }  /* carried into next binade */
            }
            uint32_t mq = (uint32_t)(sq - (1u << 22));

            /* Result exponent. The 2^23/BIAS terms cancel across the divide;
             * derivation: value = (sq/2^23) * 2^(en - ed + shift - 1). */
            int efield = (int)en - (int)ed + shift - 1 + 256;

            /* Floating overflow. The ND-500 flags FO well INSIDE the 9-bit
             * exponent field: the cross-core oracle brackets the threshold to
             * result efield in (346, 444] (346 is the largest NORMAL result
             * observed; 444 = F1 / $0x12345678 is the smallest FO). No test
             * lands inside that gap, so the exact boundary is NOT pinnable from
             * the oracle; I use true-exponent 128 (efield 384, the 2^128
             * limit) as the architecturally-plausible point - ANY value in
             * (346,444] passes the suite, and this specific choice is NOT
             * independently verified. On FO the result is forced 0 and ST gets
             * FO|O|Z = 0x4220 (oracle-verified on the $0x12345678 cases). */
            if (efield > 384) {
                nd500_write_float_register(cpu, reg_num, 0);
                cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O |
                              ND500_FLAG_DZ | ND500_FLAG_FO | ND500_FLAG_FU);
                cpu->ST1 |= ND500_FLAG_FO | ND500_FLAG_O | ND500_FLAG_Z;
                trap_floating_overflow(cpu, fi->address);
                return;
            }
            /* Floating underflow. NO cross-core case exercises the float FU
             * path, so both the threshold and the ST encoding are UNVERIFIED;
             * I mirror FO symmetrically (true exp < -128) and set FU|Z per the
             * microcode DIVFI_FU -> DIVFI_ST structure. Result forced 0. */
            if (efield < 128) {
                nd500_write_float_register(cpu, reg_num, 0);
                cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O |
                              ND500_FLAG_DZ | ND500_FLAG_FO | ND500_FLAG_FU);
                cpu->ST1 |= ND500_FLAG_FU | ND500_FLAG_Z;
                trap_floating_underflow(cpu, fi->address);
                return;
            }

            uint32_t out = (sign << 31) | (((uint32_t)efield & 0x1FF) << 22) | mq;
            nd500_write_float_register(cpu, reg_num, out);
            /* Normal result: set ONLY S from the result sign; clear the other FP
             * arithmetic status bits (Z is not set by float divide). */
            cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O |
                          ND500_FLAG_DZ | ND500_FLAG_FO | ND500_FLAG_FU);
            if (sign) {
                cpu->ST1 |= ND500_FLAG_S;
            }
            return;
        }

        /* ---------------------------------------------------------------
         * DOUBLE-precision divide (DIV_64 microcode). Still uses the host
         * double path below; its low 1-2 mantissa bits do NOT match the
         * microcode's reciprocal/SRT long-division on inexact quotients, so
         * ~12 cross-core DoubleDiv cases remain red pending a bit-level
         * DIV_64 port. Left unchanged here to avoid regressing the exact
         * cases the host path already gets right.
         * ------------------------------------------------------------- */
        double reg_value = 0.0;
        double operand_value = 0.0;

        uint64_t reg_bits = nd500_read_double_register(cpu, reg_num);
        reg_value = nd500_double_to_ieee754(reg_bits);
        uint64_t op_bits = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        operand_value = nd500_double_to_ieee754(op_bits);

        /* Check for divide by zero */
        if (operand_value == 0.0) {
            cpu->ST1 |= ND500_FLAG_DZ;
            trap_divide_by_zero(cpu, fi->address);
            return;
        }
        cpu->ST1 &= ~ND500_FLAG_DZ;

        /* Perform division */
        double result = reg_value / operand_value;

        /* Check for overflow/underflow */
        if (isinf(result) || isnan(result)) {
            trap_floating_overflow(cpu, fi->address);
        } else if (result != 0.0 && fabs(result) < 1e-38) {
            /* Underflow - result too small */
            trap_floating_underflow(cpu, fi->address);
        }

        /* Convert result back to ND-500 format */
        uint64_t result_bits = nd500_double_from_ieee754(result);
        nd500_write_double_register(cpu, reg_num, result_bits);

        /* Update flags: Z (zero), S (sign) */
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

        /* C flag unaffected for float operations */
        /* O flag unaffected (overflow handled by FO trap) */
        return;
    }

    // Integer division
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t divisor = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);

    // Check for divide by zero
    if (divisor == 0) {
        /* Non-restoring integer divide hardware leaves the quotient SATURATED at
         * the datatype's maximum positive value before the DZ trap is taken
         * (BY->0x7F, H->0x7FFF, W->0x7FFFFFFF). Match the functional CpuND500 and
         * the microword CpuND5000 (cross-core cases, nd500x commit 778d44e) - the
         * register must NOT keep its original dividend. */
        uint32_t sat;
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE:     sat = 0x0000007Fu; break;
            case ND500_DTYPE_HALFWORD: sat = 0x00007FFFu; break;
            case ND500_DTYPE_WORD:     sat = 0x7FFFFFFFu; break;
            default:                   sat = 0x7FFFFFFFu; break;
        }
        nd500_write_integer_register(cpu, fi->target_register, sat);
        cpu->ST1 |= ND500_FLAG_DZ;
        trap_divide_by_zero(cpu, fi->address);
        return;
    }

    cpu->ST1 &= ~ND500_FLAG_DZ;

    // Perform signed division with overflow detection
    int64_t quotient = 0;
    bool overflow = false;

    switch (fi->data_type) {
        case ND500_DTYPE_BYTE: {
            int8_t dividend = (int8_t)reg_value;
            int8_t div = (int8_t)divisor;
            // Check for overflow: MIN_VALUE / -1
            if (dividend == INT8_MIN && div == -1) {
                overflow = true;
                quotient = dividend;  // Keep original value
            } else {
                quotient = dividend / div;
            }
            break;
        }
        case ND500_DTYPE_HALFWORD: {
            int16_t dividend = (int16_t)reg_value;
            int16_t div = (int16_t)divisor;
            // Check for overflow: MIN_VALUE / -1
            if (dividend == INT16_MIN && div == -1) {
                overflow = true;
                quotient = dividend;  // Keep original value
            } else {
                quotient = dividend / div;
            }
            break;
        }
        case ND500_DTYPE_WORD: {
            int32_t dividend = (int32_t)reg_value;
            int32_t div = (int32_t)divisor;
            // Check for overflow: MIN_VALUE / -1
            if (dividend == INT32_MIN && div == -1) {
                overflow = true;
                quotient = dividend;  // Keep original value
            } else {
                quotient = dividend / div;
            }
            break;
        }
        default:
            printf("[ERROR] Divide at PC=0x%08X: Invalid data type %u\n",
                   fi->address, fi->data_type);
            trap_illegal_operand(cpu, fi->address);
            return;
    }

    // Mask result to data type
    uint32_t masked_result = nd500_mask_to_datatype(quotient, fi->data_type);

    // Write back to register
    nd500_write_integer_register(cpu, fi->target_register, masked_result);

    // Update status flags
    if (masked_result == 0) {
        cpu->ST1 |= ND500_FLAG_Z;
    } else {
        cpu->ST1 &= ~ND500_FLAG_Z;
    }

    if (overflow) {
        cpu->ST1 |= ND500_FLAG_O;
    } else {
        cpu->ST1 &= ~ND500_FLAG_O;
    }

    // Set sign bit based on data type
    bool sign_bit = nd500_is_negative(masked_result, fi->data_type);
    if (sign_bit) {
        cpu->ST1 |= ND500_FLAG_S;
    } else {
        cpu->ST1 &= ~ND500_FLAG_S;
    }
}
