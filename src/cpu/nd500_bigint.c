/**
 * nd500_bigint.c - a small signed multi-word integer, for float_math.c
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * See nd500_bigint.h. Plain C99: 32-bit limbs, 64-bit intermediate
 * products. Division is Knuth's Algorithm D (The Art of Computer
 * Programming, vol. 2, 4.3.1), in the form of Hacker's Delight 9-2
 * (divmnu).
 *
 * Every operation first forms its result in an arena scratch number and
 * then copies it, so the result may be one of the operands. Scratch use:
 * add/sub/mul/shift/div_u32 use scratch 0; div_trunc uses 1, 2, 3; isqrt
 * uses 4 and 5 and calls div_trunc, add and shr; 7 is handed out when the
 * arena is full.
 */

#include "nd500_bigint.h"

#include <stdlib.h>
#include <string.h>

/* ---- arena ------------------------------------------------------------ */

bool nd500_bi_arena_init(nd500_bi_arena* a, int bits, int max_count) {
    memset(a, 0, sizeof *a);
    a->cap = bits / 32 + 4;
    a->max_count = max_count;
    size_t total = (size_t)a->cap * (size_t)(max_count + ND500_BI_SCRATCH);
    a->store = (uint32_t*)calloc(total, sizeof(uint32_t));
    a->nums = (nd500_bi*)calloc((size_t)max_count, sizeof(nd500_bi));
    if (a->store == NULL || a->nums == NULL) {
        free(a->store);
        free(a->nums);
        a->store = NULL;
        a->nums = NULL;
        a->failed = true;
        return false;
    }
    for (int i = 0; i < ND500_BI_SCRATCH; i++) {
        a->scratch[i].d = a->store + (size_t)a->cap * (size_t)(max_count + i);
        a->scratch[i].n = 0;
        a->scratch[i].neg = false;
    }
    return true;
}

void nd500_bi_arena_free(nd500_bi_arena* a) {
    free(a->store);
    free(a->nums);
    a->store = NULL;
    a->nums = NULL;
    a->count = 0;
}

nd500_bi* nd500_bi_new(nd500_bi_arena* a) {
    if (a->count >= a->max_count) {
        a->failed = true;
        return &a->scratch[ND500_BI_SCRATCH - 1];
    }
    nd500_bi* r = &a->nums[a->count];
    r->d = a->store + (size_t)a->cap * (size_t)a->count;
    r->n = 0;
    r->neg = false;
    a->count++;
    return r;
}

/* ---- helpers ---------------------------------------------------------- */

static void trim(nd500_bi* r) {
    while (r->n > 0 && r->d[r->n - 1] == 0u) {
        r->n--;
    }
    if (r->n == 0) {
        r->neg = false;
    }
}

/* Room for n limbs in a number of this arena; sets failed when there is not. */
static bool room(nd500_bi_arena* a, int n) {
    if (n > a->cap) {
        a->failed = true;
        return false;
    }
    return true;
}

void nd500_bi_copy(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x) {
    (void)a;
    if (r == x) {
        return;
    }
    memcpy(r->d, x->d, (size_t)x->n * sizeof(uint32_t));
    r->n = x->n;
    r->neg = x->neg;
}

void nd500_bi_set_u64(nd500_bi_arena* a, nd500_bi* r, uint64_t v) {
    (void)a;
    r->d[0] = (uint32_t)v;
    r->d[1] = (uint32_t)(v >> 32);
    r->n = 2;
    r->neg = false;
    trim(r);
}

void nd500_bi_set_i64(nd500_bi_arena* a, nd500_bi* r, int64_t v) {
    uint64_t m = v < 0 ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
    nd500_bi_set_u64(a, r, m);
    r->neg = v < 0 && r->n > 0;
}

int nd500_bi_bitlen(const nd500_bi* x) {
    if (x->n == 0) {
        return 0;
    }
    uint32_t top = x->d[x->n - 1];
    int b = 0;
    while (top != 0u) {
        b++;
        top >>= 1;
    }
    return (x->n - 1) * 32 + b;
}

bool nd500_bi_testbit(const nd500_bi* x, int k) {
    if (k < 0 || k / 32 >= x->n) {
        return false;
    }
    return ((x->d[k / 32] >> (k % 32)) & 1u) != 0u;
}

bool nd500_bi_low_bits_nonzero(const nd500_bi* x, int k) {
    int full = k / 32;
    for (int i = 0; i < full && i < x->n; i++) {
        if (x->d[i] != 0u) {
            return true;
        }
    }
    if (full < x->n && (k % 32) != 0) {
        uint32_t mask = (1u << (k % 32)) - 1u;
        if ((x->d[full] & mask) != 0u) {
            return true;
        }
    }
    return false;
}

uint64_t nd500_bi_low_u64(const nd500_bi* x) {
    uint64_t v = 0;
    if (x->n > 0) {
        v = x->d[0];
    }
    if (x->n > 1) {
        v |= (uint64_t)x->d[1] << 32;
    }
    return v;
}

static int mag_cmp(const uint32_t* x, int xn, const uint32_t* y, int yn) {
    if (xn != yn) {
        return xn < yn ? -1 : 1;
    }
    for (int i = xn - 1; i >= 0; i--) {
        if (x[i] != y[i]) {
            return x[i] < y[i] ? -1 : 1;
        }
    }
    return 0;
}

int nd500_bi_cmpabs(const nd500_bi* x, const nd500_bi* y) {
    return mag_cmp(x->d, x->n, y->d, y->n);
}

int nd500_bi_cmp(const nd500_bi* x, const nd500_bi* y) {
    int sx = nd500_bi_sign(x), sy = nd500_bi_sign(y);
    if (sx != sy) {
        return sx < sy ? -1 : 1;
    }
    int c = nd500_bi_cmpabs(x, y);
    return sx < 0 ? -c : c;
}

/* ---- add, subtract ---------------------------------------------------- */

/* r = |x| + |y| (magnitudes). r must not be x or y. */
static void mag_add(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, const nd500_bi* y) {
    const nd500_bi* lg = x->n >= y->n ? x : y;
    const nd500_bi* sm = x->n >= y->n ? y : x;
    if (!room(a, lg->n + 1)) {
        r->n = 0;
        return;
    }
    uint64_t c = 0;
    int i = 0;
    for (; i < sm->n; i++) {
        c += (uint64_t)lg->d[i] + sm->d[i];
        r->d[i] = (uint32_t)c;
        c >>= 32;
    }
    for (; i < lg->n; i++) {
        c += lg->d[i];
        r->d[i] = (uint32_t)c;
        c >>= 32;
    }
    r->d[i] = (uint32_t)c;
    r->n = lg->n + 1;
    r->neg = false;
    trim(r);
}

/* r = |x| - |y| with |x| >= |y|. r must not be x or y. */
static void mag_sub(nd500_bi* r, const nd500_bi* x, const nd500_bi* y) {
    int64_t b = 0;
    int i = 0;
    for (; i < y->n; i++) {
        int64_t t = (int64_t)x->d[i] - y->d[i] - b;
        b = t < 0 ? 1 : 0;
        r->d[i] = (uint32_t)(t + (b << 32));
    }
    for (; i < x->n; i++) {
        int64_t t = (int64_t)x->d[i] - b;
        b = t < 0 ? 1 : 0;
        r->d[i] = (uint32_t)(t + (b << 32));
    }
    r->n = x->n;
    r->neg = false;
    trim(r);
}

/* r = x + (yneg ? -|y| : |y|). */
static void add_signed(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, const nd500_bi* y, bool yneg) {
    nd500_bi* s = &a->scratch[0];
    if (y->n == 0) {
        nd500_bi_copy(a, r, x);
        return;
    }
    if (x->n == 0) {
        nd500_bi_copy(a, r, y);
        r->neg = yneg;
        return;
    }
    if (x->neg == yneg) {
        mag_add(a, s, x, y);
        s->neg = yneg && s->n > 0;
    } else {
        int c = nd500_bi_cmpabs(x, y);
        if (c == 0) {
            s->n = 0;
            s->neg = false;
        } else if (c > 0) {
            mag_sub(s, x, y);
            s->neg = x->neg && s->n > 0;
        } else {
            mag_sub(s, y, x);
            s->neg = yneg && s->n > 0;
        }
    }
    nd500_bi_copy(a, r, s);
}

void nd500_bi_add(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, const nd500_bi* y) {
    add_signed(a, r, x, y, y->neg);
}

void nd500_bi_sub(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, const nd500_bi* y) {
    add_signed(a, r, x, y, !y->neg && y->n > 0);
}

void nd500_bi_neg(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x) {
    nd500_bi_copy(a, r, x);
    r->neg = !x->neg && x->n > 0;
}

void nd500_bi_abs(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x) {
    nd500_bi_copy(a, r, x);
    r->neg = false;
}

void nd500_bi_add_i64(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, int64_t k) {
    uint32_t limbs[2];
    nd500_bi kb;
    kb.d = limbs;
    nd500_bi_set_i64(a, &kb, k);
    nd500_bi_add(a, r, x, &kb);
}

/* ---- multiply --------------------------------------------------------- */

void nd500_bi_mul(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, const nd500_bi* y) {
    nd500_bi* s = &a->scratch[0];
    if (x->n == 0 || y->n == 0) {
        r->n = 0;
        r->neg = false;
        return;
    }
    int n = x->n + y->n;
    if (!room(a, n)) {
        r->n = 0;
        r->neg = false;
        return;
    }
    memset(s->d, 0, (size_t)n * sizeof(uint32_t));
    for (int i = 0; i < x->n; i++) {
        uint64_t c = 0;
        uint64_t xi = x->d[i];
        for (int j = 0; j < y->n; j++) {
            c += xi * y->d[j] + s->d[i + j];
            s->d[i + j] = (uint32_t)c;
            c >>= 32;
        }
        s->d[i + y->n] = (uint32_t)c;
    }
    s->n = n;
    s->neg = x->neg != y->neg;
    trim(s);
    nd500_bi_copy(a, r, s);
}

void nd500_bi_mul_i64(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, int64_t k) {
    uint32_t limbs[2];
    nd500_bi kb;
    kb.d = limbs;
    nd500_bi_set_i64(a, &kb, k);
    nd500_bi_mul(a, r, x, &kb);
}

/* ---- shifts ----------------------------------------------------------- */

void nd500_bi_shl(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, int k) {
    nd500_bi* s = &a->scratch[0];
    if (k < 0) {
        nd500_bi_shr_floor(a, r, x, -k);
        return;
    }
    if (x->n == 0) {
        r->n = 0;
        r->neg = false;
        return;
    }
    int w = k / 32, b = k % 32;
    int n = x->n + w + 1;
    if (!room(a, n)) {
        r->n = 0;
        r->neg = false;
        return;
    }
    memset(s->d, 0, (size_t)n * sizeof(uint32_t));
    for (int i = 0; i < x->n; i++) {
        uint64_t v = (uint64_t)x->d[i] << b;
        s->d[i + w] |= (uint32_t)v;
        s->d[i + w + 1] |= (uint32_t)(v >> 32);
    }
    s->n = n;
    s->neg = x->neg;
    trim(s);
    nd500_bi_copy(a, r, s);
}

void nd500_bi_shr_trunc(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, int k) {
    nd500_bi* s = &a->scratch[0];
    if (k < 0) {
        nd500_bi_shl(a, r, x, -k);
        return;
    }
    int w = k / 32, b = k % 32;
    if (w >= x->n) {
        r->n = 0;
        r->neg = false;
        return;
    }
    int n = x->n - w;
    for (int i = 0; i < n; i++) {
        uint64_t v = x->d[i + w];
        if (i + w + 1 < x->n) {
            v |= (uint64_t)x->d[i + w + 1] << 32;
        }
        s->d[i] = (uint32_t)(v >> b);
    }
    s->n = n;
    s->neg = x->neg;
    trim(s);
    nd500_bi_copy(a, r, s);
}

void nd500_bi_shr_floor(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, int k) {
    bool neg = x->neg;
    bool lost = neg && nd500_bi_low_bits_nonzero(x, k);
    nd500_bi_shr_trunc(a, r, x, k);
    if (lost) {
        nd500_bi_add_i64(a, r, r, -1);   /* a negative value cut toward zero, one step further down */
    }
}

/* ---- divide ----------------------------------------------------------- */

void nd500_bi_div_u32(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x, uint32_t k) {
    nd500_bi* s = &a->scratch[0];
    if (k == 0u) {
        a->failed = true;
        r->n = 0;
        r->neg = false;
        return;
    }
    uint64_t rem = 0;
    for (int i = x->n - 1; i >= 0; i--) {
        uint64_t cur = (rem << 32) | x->d[i];
        s->d[i] = (uint32_t)(cur / k);
        rem = cur % k;
    }
    s->n = x->n;
    s->neg = x->neg;
    trim(s);
    nd500_bi_copy(a, r, s);
}

static int leading_zeros32(uint32_t v) {
    int n = 0;
    if (v == 0u) {
        return 32;
    }
    while ((v & 0x80000000u) == 0u) {
        n++;
        v <<= 1;
    }
    return n;
}

void nd500_bi_div_trunc(nd500_bi_arena* a, nd500_bi* q, const nd500_bi* x, const nd500_bi* y) {
    nd500_bi* qs = &a->scratch[1];
    nd500_bi* un = &a->scratch[2];
    nd500_bi* vn = &a->scratch[3];
    if (y->n == 0) {
        a->failed = true;
        q->n = 0;
        q->neg = false;
        return;
    }
    bool neg = x->neg != y->neg;
    if (mag_cmp(x->d, x->n, y->d, y->n) < 0) {
        q->n = 0;
        q->neg = false;
        return;
    }
    if (y->n == 1) {
        nd500_bi_div_u32(a, q, x, y->d[0]);
        q->neg = neg && q->n > 0;
        return;
    }
    int m = x->n, n = y->n;
    if (!room(a, m + 1)) {
        q->n = 0;
        q->neg = false;
        return;
    }
    int sh = leading_zeros32(y->d[n - 1]);
    /* Normalise: vn = y << sh, un = x << sh with one more limb. */
    for (int i = n - 1; i > 0; i--) {
        vn->d[i] = sh == 0 ? y->d[i] : (y->d[i] << sh) | (y->d[i - 1] >> (32 - sh));
    }
    vn->d[0] = y->d[0] << sh;
    un->d[m] = sh == 0 ? 0u : x->d[m - 1] >> (32 - sh);
    for (int i = m - 1; i > 0; i--) {
        un->d[i] = sh == 0 ? x->d[i] : (x->d[i] << sh) | (x->d[i - 1] >> (32 - sh));
    }
    un->d[0] = x->d[0] << sh;
    const uint64_t base = (uint64_t)1 << 32;
    for (int j = m - n; j >= 0; j--) {
        uint64_t num = ((uint64_t)un->d[j + n] << 32) | un->d[j + n - 1];
        uint64_t qhat = num / vn->d[n - 1];
        uint64_t rhat = num % vn->d[n - 1];
        while (qhat >= base || qhat * vn->d[n - 2] > ((rhat << 32) | un->d[j + n - 2])) {
            qhat--;
            rhat += vn->d[n - 1];
            if (rhat >= base) {
                break;
            }
        }
        /* Multiply and subtract: un[j .. j+n] -= qhat * vn. */
        uint64_t carry = 0;
        int64_t borrow = 0;
        for (int i = 0; i < n; i++) {
            uint64_t p = qhat * vn->d[i] + carry;
            carry = p >> 32;
            int64_t t = (int64_t)un->d[i + j] - (int64_t)(uint32_t)p - borrow;
            borrow = t < 0 ? 1 : 0;
            un->d[i + j] = (uint32_t)(t + (borrow << 32));
        }
        int64_t t = (int64_t)un->d[j + n] - (int64_t)carry - borrow;
        borrow = t < 0 ? 1 : 0;
        un->d[j + n] = (uint32_t)(t + (borrow << 32));
        if (borrow) {
            /* qhat was one too big: add vn back. */
            qhat--;
            uint64_t c = 0;
            for (int i = 0; i < n; i++) {
                c += (uint64_t)un->d[i + j] + vn->d[i];
                un->d[i + j] = (uint32_t)c;
                c >>= 32;
            }
            un->d[j + n] = (uint32_t)((uint64_t)un->d[j + n] + c);
        }
        qs->d[j] = (uint32_t)qhat;
    }
    qs->n = m - n + 1;
    qs->neg = neg;
    trim(qs);
    nd500_bi_copy(a, q, qs);
}

void nd500_bi_isqrt(nd500_bi_arena* a, nd500_bi* r, const nd500_bi* x) {
    nd500_bi* g = &a->scratch[4];
    nd500_bi* y = &a->scratch[5];
    if (x->n == 0 || x->neg) {
        r->n = 0;
        r->neg = false;
        return;
    }
    /* Newton from above: g = 2**ceil(bits/2) >= sqrt(x); y = (g + x/g) / 2
     * until it stops going down. */
    nd500_bi_set_u64(a, g, 1u);
    nd500_bi_shl(a, g, g, (nd500_bi_bitlen(x) + 1) / 2);
    for (;;) {
        nd500_bi_div_trunc(a, y, x, g);
        nd500_bi_add(a, y, y, g);
        nd500_bi_shr_trunc(a, y, y, 1);
        if (nd500_bi_cmp(y, g) >= 0 || a->failed) {
            nd500_bi_copy(a, r, g);
            return;
        }
        nd500_bi_copy(a, g, y);
    }
}
