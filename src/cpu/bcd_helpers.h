/**
 * bcd_helpers.h - decimal (packed BCD and ASCII) operands
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * The decimal instructions of ND-05.009.4 chapter 17 (PADD, PSUB, PMPY,
 * PCOMP, PSHIFT, PPACK, PUPACK, PWCONV, WPCONV and their rounded forms)
 * share this module: descriptor decoding, reading and checking operands,
 * exact arithmetic on up to 31-digit values, scaling, rounding, storing
 * with BCD overflow, and the common status bits.
 *
 * Values are held as digit arrays, never as binary integers, so 31-digit
 * operands and 62-digit products are exact.
 */

#ifndef ND500_BCD_HELPERS_H
#define ND500_BCD_HELPERS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct Nd500Cpu Nd500Cpu;
typedef struct Nd500FetchedInstruction Nd500FetchedInstruction;

/* SGN field, descriptor bits 26-24 (manual chapter 17, DESCRIPTOR FORMAT). */
typedef enum {
    ND500_DEC_EMBEDDED_TRAILING = 0,
    ND500_DEC_SEPARATE_TRAILING = 1,
    ND500_DEC_EMBEDDED_LEADING  = 2,
    ND500_DEC_SEPARATE_LEADING  = 3,
    ND500_DEC_UNSIGNED          = 4
} Nd500DecSign;

/* A decoded decimal descriptor. */
typedef struct {
    uint8_t sgn;          /* bits 26-24 */
    int8_t sc;            /* bits 23-16, two's complement: digits right of the point */
    uint8_t fw;           /* field width, nibbles (BCD) or bytes (ASCII), sign included */
    uint32_t address;     /* second word: leftmost byte of the numeric field */
} Nd500DecDesc;

/* Working width: 31 digits shifted by up to 63 scale steps, or a 62-digit
 * product shifted by up to 63. */
#define ND500_DEC_CAPACITY 128

/* A decimal value: digit[0] is the least significant, and the value is
 * (negative ? -1 : 1) * sum(digit[i] * 10**i) * 10**(-sc). */
typedef struct {
    bool negative;
    int sc;
    int count;                          /* digits in use, >= 1 */
    uint8_t digit[ND500_DEC_CAPACITY];
} Nd500Dec;

/* Result of reading an operand. */
typedef enum {
    ND500_DEC_READ_OK = 0,
    ND500_DEC_READ_INVALID,   /* a non-digit in a digit position or a bad sign: IVO */
    ND500_DEC_READ_FAULT      /* an addressing trap was raised: abort the instruction */
} Nd500DecRead;

/* Read the descriptor at desc_addr. A field width of zero raises DR and
 * returns false; so does an addressing fault (the fault is already raised). */
bool nd500_dec_load_desc(Nd500Cpu* cpu, uint32_t pc, uint32_t desc_addr, Nd500DecDesc* out);

Nd500DecRead nd500_dec_read_packed(Nd500Cpu* cpu, const Nd500DecDesc* desc, Nd500Dec* out);
Nd500DecRead nd500_dec_read_ascii(Nd500Cpu* cpu, const Nd500DecDesc* desc, Nd500Dec* out);

void nd500_dec_from_int32(int32_t value, Nd500Dec* out);
bool nd500_dec_is_zero(const Nd500Dec* v);

/* a + b, or a - b when subtract is set. Exact, at the finer of the two scales. */
void nd500_dec_add(const Nd500Dec* a, const Nd500Dec* b, bool subtract, Nd500Dec* out);
/* a * b. Exact, at scale a.sc + b.sc. */
void nd500_dec_mul(const Nd500Dec* a, const Nd500Dec* b, Nd500Dec* out);
/* Sign of a - b: -1, 0 or 1. Negative and positive zero are equal. */
int nd500_dec_compare(const Nd500Dec* a, const Nd500Dec* b);

/* The add/subtract/compare restriction (manual, BCD OVERFLOW): false when
 * ((fw1+1)/2*2 - sc1) - ((fw2+1)/2*2 - sc2) is outside -32..32. */
bool nd500_dec_scale_difference_ok(const Nd500DecDesc* d1, const Nd500DecDesc* d2);

/* Store v in dest after scaling it to dest->sc, rounding when asked (manual,
 * ROUNDING). Returns false if an addressing fault was raised. *overflow is
 * set when significant digits did not fit (BO); the stored digits are then
 * the least significant ones with the sign of the correct result. *zero and
 * *negative describe the stored value for Z and S. */
bool nd500_dec_store_packed(Nd500Cpu* cpu, const Nd500DecDesc* dest, const Nd500Dec* v,
                            bool round, bool* overflow, bool* zero, bool* negative);
bool nd500_dec_store_ascii(Nd500Cpu* cpu, const Nd500DecDesc* dest, const Nd500Dec* v,
                           bool round, bool* overflow, bool* zero, bool* negative);

/* The integer part of v (fraction dropped, no rounding) as 32 bits; returns
 * true on integer overflow, when the result is the least significant 32 bits. */
bool nd500_dec_to_int32(const Nd500Dec* v, uint32_t* out);

/* Status for a decimal instruction: Z and S from the stored value, BO from
 * the overflow, K = BO or IVO (manual chapter 17, STATUS BITS). C and O are
 * reset (6.5.1). Raises the BO and IVO trap conditions. */
void nd500_dec_finish(Nd500Cpu* cpu, uint32_t pc, bool invalid, bool overflow,
                      bool zero, bool negative);

/* The instruction bodies, shared by the plain and rounded forms. */
typedef enum { ND500_DEC_ADD, ND500_DEC_SUB, ND500_DEC_MUL } Nd500DecOp;

/* PADD PADDR PSUB PSUBR PMPY PMPYR: <a> op <b> -> <c>. */
void nd500_dec_execute_arith(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                             Nd500DecOp op, bool round);
/* PCOMP: status from <a> - <b>. */
void nd500_dec_execute_compare(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);
/* PSHIFT (packed -> packed), PPACK (ASCII -> packed), PUPACK (packed ->
 * ASCII), each with its rounded form: <source> -> <dest>. */
void nd500_dec_execute_convert(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                               bool source_ascii, bool dest_ascii, bool round);
/* Wn PWCONV: packed <source> -> Rn. */
void nd500_dec_execute_pwconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);
/* Wn WPCONV: Rn -> packed <dest>. */
void nd500_dec_execute_wpconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi);

#endif /* ND500_BCD_HELPERS_H */
