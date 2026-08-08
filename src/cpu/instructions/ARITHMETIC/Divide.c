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
 * Ported from (not authoritative): RetroCore/Emulated.HW/ND/CPU/ND500/Instructions/ARITHMETIC/Divide.cs
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
            /* A faulting operand read must abort the instruction: commit nothing,
             * and raise no second trap on top of the fault the kernel is already
             * about to service. See the ADD3 guard (commit a351296) for the panic
             * this prevents. */
            if (nd500_trap_occurred() || cpu->instr_aborted) {
                return;
            }

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
         * DOUBLE-precision divide (DIV_64 microcode @023414). Exact-integer
         * model, NOT a host double division.
         *
         * DIV_64 is a NON-RESTORING binary long-division (loops DIV64L1
         * @023424 and DIV64L2 @023454, each `Q,Q*DIV ... LCDECR ... COND,AQSLZ`,
         * with LC loaded from SARG=27o=23 then SARG=36o=30). The two loops
         * generate 23+30 = 53 significand bits, so the quotient carries only
         * 53 bits of precision: the result significand is the exact quotient
         * ROUNDED TO THE NEAREST MULTIPLE OF 4 (its low two bits are zero).
         * That single fact reproduces the whole cross-core DoubleDiv oracle
         * bit-for-bit (e.g. 2/3 -> ...5554, 0.3 -> ...cccc, 0.1 -> ...6668),
         * which no simple round-to-nearest/truncate of a 55-bit quotient does
         * (they miss by a non-monotone {-1,0,+2} ULP).
         *
         * ND-500 double format: sign b63 | 9-bit exp (bias 256, b62-54) |
         * 54-bit mantissa; value = +/-(2^54+mant)/2^55 * 2^(efield-256), with
         * an implicit leading 1 at bit 54 (significand in [0.5,1)). The 64-bit
         * value is (E_reg high32 << 32) | A_reg low32.
         * ------------------------------------------------------------- */
        uint64_t nb = nd500_read_double_register(cpu, reg_num);
        uint64_t db = nd500_read_operand_doubleword(cpu, &fi->operands[0]);
        /* A faulting operand read must abort the instruction: commit nothing,
         * and raise no second trap on top of the fault the kernel is already
         * about to service. See the ADD3 guard (commit a351296) for the panic
         * this prevents. */
        if (nd500_trap_occurred() || cpu->instr_aborted) {
            return;
        }

        uint32_t en = (uint32_t)((nb >> 54) & 0x1FF);
        uint64_t mn = nb & (((uint64_t)1 << 54) - 1);
        uint32_t ed = (uint32_t)((db >> 54) & 0x1FF);
        uint64_t md = db & (((uint64_t)1 << 54) - 1);
        uint32_t dsign = (uint32_t)(((nb >> 63) & 1) ^ ((db >> 63) & 1));

        /* Divide-by-zero: divisor bits exactly zero only. Register unchanged. */
        if (ed == 0 && md == 0) {
            cpu->ST1 |= ND500_FLAG_DZ;
            trap_divide_by_zero(cpu, fi->address);
            return;
        }

        /* True-zero dividend: 0.0 / x = 0.0, Z set (double result flags come
         * from DNZRO64's ST,SAVA, which unlike the single path DOES set Z). */
        if (en == 0 && mn == 0) {
            nd500_write_double_register(cpu, reg_num, 0);
            cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O |
                          ND500_FLAG_DZ | ND500_FLAG_FO | ND500_FLAG_FU);
            cpu->ST1 |= ND500_FLAG_Z;
            return;
        }

        /* Unnormalised (denormal) dividend, efield==0 with a nonzero mantissa.
         * DIV_64 does not renormalise it; the result is forced to zero and the
         * flag depends on whether the DIVISOR is normalised (cross-core oracle,
         * dividend 0x000000003FF00000):
         *   - normalised divisor (ed != 0): the tiny dividend underflows to a
         *     clean zero -> Z set (st=0x20).
         *   - unnormalised divisor (ed == 0, md != 0): both operands are
         *     unnormalised -> S set (st=0x80), the ND "unnormalised operand"
         *     marker rather than a true-zero Z.
         * (The exact-zero divisor was already handled as DZ above.) NOTE: all
         * oracle cases share the one dividend 0x3FF00000, so the value/threshold
         * details of the general denormal path are not fully pinned - only the
         * ed==0-vs-ed!=0 flag split is verified. */
        if (en == 0) {
            nd500_write_double_register(cpu, reg_num, 0);
            cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O |
                          ND500_FLAG_DZ | ND500_FLAG_FO | ND500_FLAG_FU);
            cpu->ST1 |= (ed != 0) ? ND500_FLAG_Z : ND500_FLAG_S;
            return;
        }

        /* Normalised operands: exact long-divide with round-to-nearest-mult-4.
         * N,D are the 55-bit significands (implicit leading 1 at bit 54). */
        unsigned __int128 N = (unsigned __int128)(((uint64_t)1 << 54) | mn);
        unsigned __int128 D = (unsigned __int128)(((uint64_t)1 << 54) | md);
        unsigned __int128 QQ = (N << 57) / D;   /* ratio_sig * 2^57 */
        unsigned __int128 RR = (N << 57) % D;    /* stickiness -> nonzero remainder */

        /* Bit length of QQ (positive, < 2^112). */
        uint64_t qq_hi = (uint64_t)(QQ >> 64);
        uint64_t qq_lo = (uint64_t)QQ;
        int bitlen = qq_hi ? (128 - __builtin_clzll(qq_hi)) : (64 - __builtin_clzll(qq_lo));
        int top = bitlen - 1;
        int shift = top - 54;                    /* in {2,3} -> shift-2 >= 0 */

        uint64_t fq = (uint64_t)(QQ >> shift);   /* 55-bit floor significand */
        unsigned __int128 fracbits = QQ & ((((unsigned __int128)1) << shift) - 1);

        /* Round the 55-bit significand to the nearest multiple of 4 (the 53-bit
         * grid the two DIV_64 loops produce). Half rounds up when any lower bit
         * or the division remainder is nonzero (sticky). */
        uint64_t q4 = fq >> 2;
        uint64_t r4 = fq & 3;
        unsigned __int128 frac_num = ((unsigned __int128)r4 << shift) + fracbits;
        unsigned __int128 denom = (unsigned __int128)4 << shift;
        int sticky = (fracbits != 0 || RR != 0);
        if (2 * frac_num > denom || (2 * frac_num == denom && sticky)) {
            q4++;
        }
        uint64_t sres = q4 << 2;
        if (sres >= ((uint64_t)1 << 55)) { sres >>= 1; shift++; }   /* carried up a binade */

        /* value = sig * 2^(en - ed + shift - 2); efield = that + bias 256. */
        int efield = (int)en - (int)ed + shift - 2 + 256;

        /* Floating overflow: same architectural sub-9-bit threshold as single;
         * no double case exercises it so this is by analogy, not verified. */
        if (efield > 384) {
            nd500_write_double_register(cpu, reg_num, 0);
            cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O |
                          ND500_FLAG_DZ | ND500_FLAG_FO | ND500_FLAG_FU);
            cpu->ST1 |= ND500_FLAG_FO | ND500_FLAG_O | ND500_FLAG_Z;
            trap_floating_overflow(cpu, fi->address);
            return;
        }
        if (efield < 128) {
            nd500_write_double_register(cpu, reg_num, 0);
            cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O |
                          ND500_FLAG_DZ | ND500_FLAG_FO | ND500_FLAG_FU);
            cpu->ST1 |= ND500_FLAG_FU | ND500_FLAG_Z;
            trap_floating_underflow(cpu, fi->address);
            return;
        }

        uint64_t mant = sres & (((uint64_t)1 << 54) - 1);
        uint64_t out = ((uint64_t)dsign << 63) |
                       ((uint64_t)((uint32_t)efield & 0x1FF) << 54) | mant;
        nd500_write_double_register(cpu, reg_num, out);

        /* Normal result flags (DNZRO64 ST,SAVA): Z if the result is zero,
         * else S from the result sign. */
        cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_O |
                      ND500_FLAG_DZ | ND500_FLAG_FO | ND500_FLAG_FU);
        if ((out & 0x7FFFFFFFFFFFFFFFULL) == 0) {
            cpu->ST1 |= ND500_FLAG_Z;
        } else if (dsign) {
            cpu->ST1 |= ND500_FLAG_S;
        }
        return;
    }

    // Integer division
    uint32_t reg_value = nd500_read_integer_register(cpu, fi->target_register);
    uint64_t divisor = nd500_read_operand_value(cpu, &fi->operands[0], fi->data_type);
    /* A faulting operand read must abort the instruction: commit nothing,
     * and raise no second trap on top of the fault the kernel is already
     * about to service. See the ADD3 guard (commit a351296) for the panic
     * this prevents. */
    if (nd500_trap_occurred() || cpu->instr_aborted) {
        return;
    }

    // Check for divide by zero
    if (divisor == 0) {
        /* Non-restoring integer divide hardware leaves the quotient SATURATED at the
         * LARGEST-MAGNITUDE value OF THE DIVIDEND'S SIGN before the DZ trap is taken -
         * NOT unconditionally max positive. Adjudicated against the real B30 microcode
         * (2026-07-26): divide-by-zero BRANCHES on the dividend sign at @024133
         * (COND,MSGN -> INTDN):
         *   - POSITIVE dividend -> @024134-024135 (OR A,BM14=DZ bit12 ST,LOAD):
         *     quotient = max positive (BY 0x7F, H 0x7FFF, W 0x7FFFFFFF), S = 0.
         *   - NEGATIVE dividend -> INTDN @024136-024140 (OR A,SARG=010200 = DZ bit12 +
         *     S bit7 ST,LOAD): quotient = MIN (BY 0x80, H 0x8000, W 0x80000000), S = 1.
         * Traced microword: -12/0 -> 0x80000000 S=1; +5/0 -> 0x7FFFFFFF S=0. The earlier
         * "always max positive" fix (commit ddda6ea/778d44e) only covered positive
         * dividends. S follows the saturated quotient's sign (= the dividend sign). */
        bool dividend_negative;
        uint32_t sat;
        switch (fi->data_type) {
            case ND500_DTYPE_BYTE:
                dividend_negative = (reg_value & 0x80u) != 0;
                sat = dividend_negative ? 0x00000080u : 0x0000007Fu;
                break;
            case ND500_DTYPE_HALFWORD:
                dividend_negative = (reg_value & 0x8000u) != 0;
                sat = dividend_negative ? 0x00008000u : 0x00007FFFu;
                break;
            case ND500_DTYPE_WORD:
            default:
                dividend_negative = (reg_value & 0x80000000u) != 0;
                sat = dividend_negative ? 0x80000000u : 0x7FFFFFFFu;
                break;
        }
        nd500_write_integer_register(cpu, fi->target_register, sat);
        if (dividend_negative) {
            cpu->ST1 |= ND500_FLAG_S;   /* microcode INTDN ST,LOAD SARG=010200 sets S */
        } else {
            cpu->ST1 &= ~ND500_FLAG_S;
        }
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
