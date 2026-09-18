/**
 * bcd_helpers.c - decimal (packed BCD and ASCII) operands
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Every rule below is from ND-05.009.4 chapter 17 (BINARY CODED DECIMAL
 * INSTRUCTIONS). Where the manual is silent the choice is marked
 * "not in the manual" so it can be settled later.
 */

#include "bcd_helpers.h"
#include "cpu_protos.h"
#include "instruction_helpers.h"
#include <string.h>

/* Room for a value scaled by up to 95 steps (see nd500_dec_store_*). */
#define DEC_WORK 256

static void dec_zero(Nd500Dec* v, int sc) {
    memset(v, 0, sizeof(*v));
    v->count = 1;
    v->sc = sc;
}

/* Drop leading zero digits; a zero value is positive. */
static void dec_normalise(Nd500Dec* v) {
    while (v->count > 1 && v->digit[v->count - 1] == 0) {
        v->count--;
    }
    if (v->count == 1 && v->digit[0] == 0) {
        v->negative = false;
    }
}

bool nd500_dec_is_zero(const Nd500Dec* v) {
    for (int i = 0; i < v->count; i++) {
        if (v->digit[i] != 0) return false;
    }
    return true;
}

static bool dec_faulted(Nd500Cpu* cpu) {
    return nd500_trap_occurred() || cpu->instr_aborted;
}

/* ------------------------------------------------------------------------
 * Descriptor: SGN bits 26-24, SC bits 23-16 (two's complement byte), FW
 * bits 15-0 with range 0..31, then the address word (manual DESCRIPTOR
 * FORMAT FOR ASCII AND BCD). The B30 microcode takes FW as the low five
 * bits (001022B AND 37B). "A field width of zero will cause a
 * descriptor-range trap condition. The address is not checked."
 * ---------------------------------------------------------------------- */
bool nd500_dec_load_desc(Nd500Cpu* cpu, uint32_t pc, uint32_t desc_addr, Nd500DecDesc* out) {
    uint32_t w0 = nd500_read_memory_32(cpu, desc_addr);
    uint32_t w1 = nd500_read_memory_32(cpu, desc_addr + 4);
    if (dec_faulted(cpu)) {
        return false;
    }
    out->sgn = (uint8_t)((w0 >> 24) & 0x07);
    out->sc = (int8_t)((w0 >> 16) & 0xFF);
    out->fw = (uint8_t)(w0 & 0x1F);
    out->address = w1;
    if (out->fw == 0) {
        trap_descriptor_range(cpu, pc);
        return false;
    }
    return true;
}

bool nd500_dec_scale_difference_ok(const Nd500DecDesc* d1, const Nd500DecDesc* d2) {
    int t1 = (d1->fw + 1) / 2 * 2 - d1->sc;
    int t2 = (d2->fw + 1) / 2 * 2 - d2->sc;
    int d = t1 - t2;
    return d >= -32 && d <= 32;
}

/* ------------------------------------------------------------------------
 * Packed: FW nibbles including the sign, right justified in (FW+1)/2
 * bytes; with FW odd the leftmost nibble is not significant. Digits are
 * 0000-1001; the sign is the rightmost nibble: + is 0000 1010 1100 1110,
 * - is 1011 1101, unsigned 1111 (treated as plus). Anything else is an
 * invalid operation.
 * ---------------------------------------------------------------------- */
Nd500DecRead nd500_dec_read_packed(Nd500Cpu* cpu, const Nd500DecDesc* desc, Nd500Dec* out) {
    uint32_t bytes = ((uint32_t)desc->fw + 1) / 2;
    uint8_t nib[32];
    for (uint32_t i = 0; i < bytes; i++) {
        uint8_t b = nd500_read_memory_8(cpu, desc->address + i);
        if (dec_faulted(cpu)) {
            return ND500_DEC_READ_FAULT;
        }
        nib[2 * i] = (uint8_t)(b >> 4);
        nib[2 * i + 1] = (uint8_t)(b & 0x0F);
    }
    uint32_t total = bytes * 2;
    uint32_t first = total - desc->fw;          /* 1 when FW is odd */
    uint8_t sign = nib[total - 1];

    dec_zero(out, desc->sc);
    bool invalid = (sign >= 1 && sign <= 9);
    out->negative = (sign == 0x0B || sign == 0x0D);

    int n = 0;
    for (uint32_t i = total - 2; i + 1 > first; i--) {   /* least significant first */
        uint8_t d = nib[i];
        if (d > 9) invalid = true;
        out->digit[n++] = (uint8_t)(d > 9 ? 0 : d);
        if (i == 0) break;
    }
    out->count = n > 0 ? n : 1;
    dec_normalise(out);
    return invalid ? ND500_DEC_READ_INVALID : ND500_DEC_READ_OK;
}

/* ------------------------------------------------------------------------
 * ASCII: one byte a digit, zone 0011. Embedded sign ("overpunch"): plus
 * 0 = 173B (07BH), 1..9 = 101B..111B (041H..049H), minus 0 = 175B (07DH),
 * 1..9 = 112B..122B (04AH..052H); a plain ASCII digit there is plus. A
 * separate sign is + (040B/020H or 053B/02BH) or - (055B/02DH). The parity
 * bit is ignored on input ("with or without parity"); not in the manual:
 * whether that holds for plain digit bytes too - it is applied to all.
 * ---------------------------------------------------------------------- */
static bool ascii_digit(uint8_t b, uint8_t* d) {
    if (b >= 0x30 && b <= 0x39) { *d = (uint8_t)(b - 0x30); return true; }
    return false;
}

static bool ascii_embedded(uint8_t b, uint8_t* d, bool* negative) {
    if (ascii_digit(b, d)) { *negative = false; return true; }
    if (b == 0x7B) { *d = 0; *negative = false; return true; }
    if (b >= 0x41 && b <= 0x49) { *d = (uint8_t)(b - 0x40); *negative = false; return true; }
    if (b == 0x7D) { *d = 0; *negative = true; return true; }
    if (b >= 0x4A && b <= 0x52) { *d = (uint8_t)(b - 0x49); *negative = true; return true; }
    return false;
}

static bool ascii_separate(uint8_t b, bool* negative) {
    if (b == 0x20 || b == 0x2B) { *negative = false; return true; }
    if (b == 0x2D) { *negative = true; return true; }
    return false;
}

Nd500DecRead nd500_dec_read_ascii(Nd500Cpu* cpu, const Nd500DecDesc* desc, Nd500Dec* out) {
    uint8_t raw[32];
    for (uint32_t i = 0; i < desc->fw; i++) {
        raw[i] = (uint8_t)(nd500_read_memory_8(cpu, desc->address + i) & 0x7F);
        if (dec_faulted(cpu)) {
            return ND500_DEC_READ_FAULT;
        }
    }
    dec_zero(out, desc->sc);
    if (desc->sgn > ND500_DEC_UNSIGNED) {
        return ND500_DEC_READ_INVALID;
    }

    int fw = desc->fw;
    int lo = 0, hi = fw;          /* digit bytes are raw[lo..hi-1] */
    int embedded = -1;            /* index of the byte holding digit and sign */
    bool invalid = false;
    bool negative = false;
    switch (desc->sgn) {
        case ND500_DEC_EMBEDDED_TRAILING: embedded = fw - 1; break;
        case ND500_DEC_EMBEDDED_LEADING:  embedded = 0;      break;
        case ND500_DEC_SEPARATE_TRAILING:
            hi = fw - 1;
            if (!ascii_separate(raw[fw - 1], &negative)) invalid = true;
            break;
        case ND500_DEC_SEPARATE_LEADING:
            lo = 1;
            if (!ascii_separate(raw[0], &negative)) invalid = true;
            break;
        default:  /* unsigned: a sign code in any position is invalid */
            break;
    }

    int n = 0;
    for (int i = hi - 1; i >= lo; i--) {
        uint8_t d = 0;
        bool ok;
        if (i == embedded) {
            ok = ascii_embedded(raw[i], &d, &negative);
        } else {
            ok = ascii_digit(raw[i], &d);
        }
        if (!ok) { invalid = true; d = 0; }
        out->digit[n++] = d;
    }
    out->count = n > 0 ? n : 1;
    out->negative = negative;
    dec_normalise(out);
    return invalid ? ND500_DEC_READ_INVALID : ND500_DEC_READ_OK;
}

void nd500_dec_from_int32(int32_t value, Nd500Dec* out) {
    dec_zero(out, 0);
    uint64_t mag = value < 0 ? (uint64_t)(-(int64_t)value) : (uint64_t)value;
    out->negative = value < 0;
    int n = 0;
    do {
        out->digit[n++] = (uint8_t)(mag % 10);
        mag /= 10;
    } while (mag != 0);
    out->count = n;
    dec_normalise(out);
}

/* ------------------------------------------------------------------------
 * Arithmetic
 * ---------------------------------------------------------------------- */

/* v's digits at scale sc >= v->sc, into buf (least significant first). */
static int dec_align(const Nd500Dec* v, int sc, uint8_t* buf) {
    int shift = sc - v->sc;
    memset(buf, 0, DEC_WORK);
    for (int i = 0; i < v->count && i + shift < DEC_WORK; i++) {
        buf[i + shift] = v->digit[i];
    }
    int n = v->count + shift;
    return n < DEC_WORK ? n : DEC_WORK;
}

static int mag_compare(const uint8_t* a, const uint8_t* b, int n) {
    for (int i = n - 1; i >= 0; i--) {
        if (a[i] != b[i]) return a[i] > b[i] ? 1 : -1;
    }
    return 0;
}

static void dec_from_buf(const uint8_t* buf, int n, int sc, bool negative, Nd500Dec* out) {
    dec_zero(out, sc);
    if (n > ND500_DEC_CAPACITY) n = ND500_DEC_CAPACITY;
    memcpy(out->digit, buf, (size_t)n);
    out->count = n > 0 ? n : 1;
    out->negative = negative;
    dec_normalise(out);
}

void nd500_dec_add(const Nd500Dec* a, const Nd500Dec* b, bool subtract, Nd500Dec* out) {
    uint8_t x[DEC_WORK], y[DEC_WORK], r[DEC_WORK];
    int sc = a->sc > b->sc ? a->sc : b->sc;
    int nx = dec_align(a, sc, x);
    int ny = dec_align(b, sc, y);
    int n = (nx > ny ? nx : ny) + 1;
    if (n > DEC_WORK) n = DEC_WORK;
    bool na = a->negative;
    bool nb = subtract ? !b->negative : b->negative;
    memset(r, 0, sizeof(r));

    if (na == nb) {
        int carry = 0;
        for (int i = 0; i < n; i++) {
            int s = x[i] + y[i] + carry;
            r[i] = (uint8_t)(s % 10);
            carry = s / 10;
        }
        dec_from_buf(r, n, sc, na, out);
        return;
    }
    int c = mag_compare(x, y, n);
    const uint8_t* big = c >= 0 ? x : y;
    const uint8_t* small = c >= 0 ? y : x;
    bool negative = c >= 0 ? na : nb;
    int borrow = 0;
    for (int i = 0; i < n; i++) {
        int s = big[i] - small[i] - borrow;
        borrow = s < 0;
        r[i] = (uint8_t)(s < 0 ? s + 10 : s);
    }
    dec_from_buf(r, n, sc, negative, out);
}

void nd500_dec_mul(const Nd500Dec* a, const Nd500Dec* b, Nd500Dec* out) {
    uint32_t acc[DEC_WORK];
    uint8_t r[DEC_WORK];
    memset(acc, 0, sizeof(acc));
    for (int i = 0; i < a->count; i++) {
        for (int j = 0; j < b->count && i + j < DEC_WORK; j++) {
            acc[i + j] += (uint32_t)a->digit[i] * b->digit[j];
        }
    }
    uint32_t carry = 0;
    for (int i = 0; i < DEC_WORK; i++) {
        uint32_t s = acc[i] + carry;
        r[i] = (uint8_t)(s % 10);
        carry = s / 10;
    }
    dec_from_buf(r, a->count + b->count, a->sc + b->sc, a->negative != b->negative, out);
}

int nd500_dec_compare(const Nd500Dec* a, const Nd500Dec* b) {
    Nd500Dec d;
    nd500_dec_add(a, b, true, &d);
    if (nd500_dec_is_zero(&d)) return 0;
    return d.negative ? -1 : 1;
}

/* ------------------------------------------------------------------------
 * Storing: scale to the destination, round if asked ("the leftmost digit
 * not stored is inspected. If this digit is 5, 6, 7, 8 or 9 the least
 * significant digit actually stored is incremented by 1"), then keep
 * cap digits. More significant nonzero digits are a BCD overflow: "the
 * result is replaced by the correctly signed least significant digits".
 * A zero result is positive; a result that is zero only because digits
 * were lost keeps its sign, and Z=1 S=0 are set for it (NEGATIVE AND
 * POSITIVE ZERO).
 * ---------------------------------------------------------------------- */
typedef struct {
    uint8_t digit[32];     /* the stored digits, least significant first */
    bool negative;         /* sign of the correct (rounded) result */
    bool overflow;
    bool zero;             /* the stored digits are all zero */
} DecStored;

static void dec_fit(const Nd500Dec* v, int sc, bool round, int cap, DecStored* s) {
    uint8_t buf[DEC_WORK];
    memset(buf, 0, sizeof(buf));
    int n;
    int shift = sc - v->sc;
    if (shift >= 0) {
        for (int i = 0; i < v->count && i + shift < DEC_WORK; i++) buf[i + shift] = v->digit[i];
        n = v->count + shift;
    } else {
        int drop = -shift;
        for (int i = drop; i < v->count; i++) buf[i - drop] = v->digit[i];
        n = v->count - drop;
        bool up = round && drop - 1 < v->count && v->digit[drop - 1] >= 5;
        for (int i = 0; up && i < DEC_WORK; i++) {
            if (buf[i] == 9) { buf[i] = 0; } else { buf[i]++; up = false; }
        }
        n++;
    }
    if (n > DEC_WORK) n = DEC_WORK;
    if (n < 1) n = 1;

    bool all_zero = true;
    for (int i = 0; i < n; i++) if (buf[i] != 0) { all_zero = false; break; }
    s->negative = v->negative && !all_zero;
    s->overflow = false;
    for (int i = cap; i < n; i++) if (buf[i] != 0) { s->overflow = true; break; }
    s->zero = true;
    memset(s->digit, 0, sizeof(s->digit));
    for (int i = 0; i < cap && i < 32; i++) {
        s->digit[i] = buf[i];
        if (buf[i] != 0) s->zero = false;
    }
}

bool nd500_dec_store_packed(Nd500Cpu* cpu, const Nd500DecDesc* dest, const Nd500Dec* v,
                            bool round, bool* overflow, bool* zero, bool* negative) {
    int cap = dest->fw - 1;
    DecStored s;
    dec_fit(v, dest->sc, round, cap, &s);

    /* "If bit 26 in the descriptor of the <dest> operand is set, the value is
     * stored with a sign code equal to 1111"; results otherwise use 1100 for
     * plus and 1101 for minus. */
    bool unsigned_dest = (dest->sgn & 0x04) != 0;
    uint8_t sign = unsigned_dest ? 0x0F : (s.negative ? 0x0D : 0x0C);

    uint32_t bytes = ((uint32_t)dest->fw + 1) / 2;
    uint32_t total = bytes * 2;
    uint8_t nib[32];
    memset(nib, 0, sizeof(nib));
    nib[total - 1] = sign;
    for (int i = 0; i < cap; i++) {
        nib[total - 2 - (uint32_t)i] = s.digit[i];
    }
    for (uint32_t i = 0; i < bytes; i++) {
        nd500_write_memory_8(cpu, dest->address + i, (uint8_t)((nib[2 * i] << 4) | nib[2 * i + 1]));
        if (dec_faulted(cpu)) {
            return false;
        }
    }
    *overflow = s.overflow;
    *zero = s.zero;
    /* Not in the manual: S for an unsigned destination. The stored value has
     * no sign, so S is left clear. */
    *negative = !s.zero && s.negative && !unsigned_dest;
    return true;
}

bool nd500_dec_store_ascii(Nd500Cpu* cpu, const Nd500DecDesc* dest, const Nd500Dec* v,
                           bool round, bool* overflow, bool* zero, bool* negative) {
    int fw = dest->fw;
    bool separate = dest->sgn == ND500_DEC_SEPARATE_TRAILING || dest->sgn == ND500_DEC_SEPARATE_LEADING;
    int cap = separate ? fw - 1 : fw;
    DecStored s;
    dec_fit(v, dest->sc, round, cap, &s);
    bool signless = dest->sgn == ND500_DEC_UNSIGNED;

    /* Digits, most significant first, "extended with leading ASCII zeros",
     * parity bit zero. */
    uint8_t out[32];
    int lo = dest->sgn == ND500_DEC_SEPARATE_LEADING ? 1 : 0;
    for (int i = 0; i < cap; i++) {
        out[lo + cap - 1 - i] = (uint8_t)(0x30 + s.digit[i]);
    }
    /* Not in the manual: which of the two plus codes is written for a
     * separate sign. 053B ('+') is used. */
    uint8_t sign_byte = s.negative ? 0x2D : 0x2B;
    switch (dest->sgn) {
        case ND500_DEC_SEPARATE_TRAILING: out[fw - 1] = sign_byte; break;
        case ND500_DEC_SEPARATE_LEADING:  out[0] = sign_byte;      break;
        case ND500_DEC_EMBEDDED_TRAILING:
        case ND500_DEC_EMBEDDED_LEADING: {
            int at = dest->sgn == ND500_DEC_EMBEDDED_TRAILING ? fw - 1 : 0;
            uint8_t d = (uint8_t)(out[at] - 0x30);
            if (s.negative) {
                out[at] = d == 0 ? 0x7D : (uint8_t)(0x49 + d);
            } else {
                out[at] = d == 0 ? 0x7B : (uint8_t)(0x40 + d);
            }
            break;
        }
        default:
            break;
    }
    for (int i = 0; i < fw; i++) {
        nd500_write_memory_8(cpu, dest->address + (uint32_t)i, out[i]);
        if (dec_faulted(cpu)) {
            return false;
        }
    }
    *overflow = s.overflow;
    *zero = s.zero;
    *negative = !s.zero && s.negative && !signless;
    return true;
}

bool nd500_dec_to_int32(const Nd500Dec* v, uint32_t* out) {
    uint32_t low = 0;          /* magnitude modulo 2**32 */
    uint64_t mag = 0;          /* exact magnitude until it passes 2**33 */
    bool big = false;
    /* Integer digits: indexes >= sc; a negative sc appends -sc zeros. */
    int top = v->count - 1;
    int bottom = v->sc > 0 ? v->sc : 0;
    int zeros = v->sc < 0 ? -v->sc : 0;
    for (int i = top; i >= bottom; i--) {
        low = low * 10u + v->digit[i];
        if (!big) { mag = mag * 10u + v->digit[i]; if (mag > (1ull << 33)) big = true; }
    }
    for (int i = 0; i < zeros; i++) {
        low *= 10u;
        if (!big) { mag *= 10u; if (mag > (1ull << 33)) big = true; }
    }
    bool overflow = big || (v->negative ? mag > 0x80000000ull : mag > 0x7FFFFFFFull);
    *out = v->negative ? (uint32_t)(0u - low) : low;
    return overflow;
}

void nd500_dec_finish(Nd500Cpu* cpu, uint32_t pc, bool invalid, bool overflow,
                      bool zero, bool negative) {
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O | ND500_FLAG_K);
    /* Not in the manual: Z and S after an invalid operation, when nothing
     * is stored. They are left clear. */
    if (!invalid) {
        if (zero) cpu->ST1 |= ND500_FLAG_Z;
        else if (negative) cpu->ST1 |= ND500_FLAG_S;
    }
    if (invalid || overflow) {
        cpu->ST1 |= ND500_FLAG_K;
    }
    if (overflow) {
        trap_bcd_overflow(cpu, pc);
    }
    if (invalid) {
        trap_invalid_operation(cpu, pc);
    }
}

/* ------------------------------------------------------------------------
 * Instruction bodies. "Descriptor addressing is implicit": each operand's
 * effective address is the address of its descriptor. All sources are read
 * before the destination is written, so an operand may be both (OPERAND
 * OVERLAP). Nothing is stored after an invalid operation (not in the
 * manual).
 * ---------------------------------------------------------------------- */
static bool dec_operands(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi, unsigned count,
                         Nd500DecDesc* desc) {
    if (fi->operand_count != count) {
        trap_illegal_operand(cpu, fi->address);
        return false;
    }
    for (unsigned i = 0; i < count; i++) {
        if (!nd500_dec_load_desc(cpu, fi->address, fi->operands[i].effective_address, &desc[i])) {
            return false;
        }
    }
    return true;
}

void nd500_dec_execute_arith(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                             Nd500DecOp op, bool round) {
    Nd500DecDesc d[3];
    Nd500Dec a, b, r;
    if (!dec_operands(cpu, fi, 3, d)) return;
    Nd500DecRead ra = nd500_dec_read_packed(cpu, &d[0], &a);
    if (ra == ND500_DEC_READ_FAULT) return;
    Nd500DecRead rb = nd500_dec_read_packed(cpu, &d[1], &b);
    if (rb == ND500_DEC_READ_FAULT) return;

    bool invalid = ra != ND500_DEC_READ_OK || rb != ND500_DEC_READ_OK;
    if (op == ND500_DEC_MUL) {
        /* "For PMPY/PMPYR, an operand with invalid digit * ZRO gives the
         * result 0, not IVO." */
        if ((ra == ND500_DEC_READ_OK && nd500_dec_is_zero(&a)) ||
            (rb == ND500_DEC_READ_OK && nd500_dec_is_zero(&b))) {
            invalid = false;
        }
        nd500_dec_mul(&a, &b, &r);
    } else {
        /* The add/subtract restriction on the scaling difference. */
        if (!nd500_dec_scale_difference_ok(&d[0], &d[1])) invalid = true;
        nd500_dec_add(&a, &b, op == ND500_DEC_SUB, &r);
    }
    bool overflow = false, zero = false, negative = false;
    if (!invalid && !nd500_dec_store_packed(cpu, &d[2], &r, round, &overflow, &zero, &negative)) {
        return;
    }
    nd500_dec_finish(cpu, fi->address, invalid, overflow, zero, negative);
}

void nd500_dec_execute_compare(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    Nd500DecDesc d[2];
    Nd500Dec a, b;
    if (!dec_operands(cpu, fi, 2, d)) return;
    Nd500DecRead ra = nd500_dec_read_packed(cpu, &d[0], &a);
    if (ra == ND500_DEC_READ_FAULT) return;
    Nd500DecRead rb = nd500_dec_read_packed(cpu, &d[1], &b);
    if (rb == ND500_DEC_READ_FAULT) return;
    bool invalid = ra != ND500_DEC_READ_OK || rb != ND500_DEC_READ_OK ||
                   !nd500_dec_scale_difference_ok(&d[0], &d[1]);
    /* "An unsigned number is treated as positive, and positive and negative
     * zero are equal." */
    int c = nd500_dec_compare(&a, &b);
    nd500_dec_finish(cpu, fi->address, invalid, false, c == 0, c < 0);
}

void nd500_dec_execute_convert(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi,
                               bool source_ascii, bool dest_ascii, bool round) {
    Nd500DecDesc d[2];
    Nd500Dec v;
    if (!dec_operands(cpu, fi, 2, d)) return;
    Nd500DecRead rv = source_ascii ? nd500_dec_read_ascii(cpu, &d[0], &v)
                                   : nd500_dec_read_packed(cpu, &d[0], &v);
    if (rv == ND500_DEC_READ_FAULT) return;
    bool invalid = rv != ND500_DEC_READ_OK || (dest_ascii && d[1].sgn > ND500_DEC_UNSIGNED);
    bool overflow = false, zero = false, negative = false;
    if (!invalid) {
        bool ok = dest_ascii
            ? nd500_dec_store_ascii(cpu, &d[1], &v, round, &overflow, &zero, &negative)
            : nd500_dec_store_packed(cpu, &d[1], &v, round, &overflow, &zero, &negative);
        if (!ok) return;
    }
    nd500_dec_finish(cpu, fi->address, invalid, overflow, zero, negative);
}

void nd500_dec_execute_pwconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    Nd500DecDesc d;
    Nd500Dec v;
    if (!dec_operands(cpu, fi, 1, &d)) return;
    Nd500DecRead rv = nd500_dec_read_packed(cpu, &d, &v);
    if (rv == ND500_DEC_READ_FAULT) return;
    bool invalid = rv != ND500_DEC_READ_OK;
    uint32_t value = 0;
    bool overflow = false;
    if (!invalid) {
        /* "The fractional part of <source> is lost; no rounding is performed
         * ... On integer overflow the result is the least significant 32
         * bits of the binary result." */
        overflow = nd500_dec_to_int32(&v, &value);
        nd500_write_integer_register(cpu, fi->target_register, value);
    }
    /* value = 0 -> Z, value.signbit -> S, overflow -> O, IVO or O -> K. */
    cpu->ST1 &= ~(ND500_FLAG_Z | ND500_FLAG_S | ND500_FLAG_C | ND500_FLAG_O | ND500_FLAG_K);
    if (!invalid) {
        if (value == 0) cpu->ST1 |= ND500_FLAG_Z;
        if (value & 0x80000000u) cpu->ST1 |= ND500_FLAG_S;
    }
    if (overflow) cpu->ST1 |= ND500_FLAG_O;
    if (invalid || overflow) cpu->ST1 |= ND500_FLAG_K;
    if (overflow) trap_integer_overflow(cpu, fi->address);
    if (invalid) trap_invalid_operation(cpu, fi->address);
}

void nd500_dec_execute_wpconv(Nd500Cpu* cpu, const Nd500FetchedInstruction* fi) {
    Nd500DecDesc d;
    Nd500Dec v;
    if (!dec_operands(cpu, fi, 1, &d)) return;
    nd500_dec_from_int32((int32_t)nd500_read_integer_register(cpu, fi->target_register), &v);
    /* "If the scaling factor of <dest> is negative, the least significant
     * digits are lost" - no rounding. */
    bool overflow = false, zero = false, negative = false;
    if (!nd500_dec_store_packed(cpu, &d, &v, false, &overflow, &zero, &negative)) return;
    nd500_dec_finish(cpu, fi->address, false, overflow, zero, negative);
}
