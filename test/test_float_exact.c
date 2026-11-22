/*
 * test_float_exact.c - src/cpu/float_exact.c against an independent reference
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * Add, subtract, multiply, divide and square root. The vectors in float_exact_vectors.h come from tools/gen_float_exact_vectors.py,
 * which computes each result with exact rational arithmetic and rounds it by
 * ND-05.009.4 7.2.7, with the overflow and underflow results of 6.5.1.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "float_exact.h"
#include "float_exact_vectors.h"

/* nd500_fx_text: the exact decimal of the bits. The expected texts were worked out with exact
 * rational arithmetic (Python fractions) from the layout of ND-05.009.4 7.2.5/7.2.6: 1.0,
 * 2.5, -0.375, the single nearest 0.1 (23 bits: 6710886 / 2**26), 2.5 as a double, a double
 * of 57 digits and the largest and the smallest single (cut to 40 digits, "..."), and two
 * zeros (exponent 0, one with a sign and a mantissa). */
static const struct { uint64_t bits; int is_double; const char* text; } k_texts[] = {
    { 0x40400000ULL, 0, "1" },
    { 0x40900000ULL, 0, "2.5" },
    { 0xBFE00000ULL, 0, "-0.375" },
    { 0x3F666666ULL, 0, "0.0999999940395355224609375" },
    { 0x4090000000000000ULL, 1, "2.5" },
    { 0x3F4999999999999AULL, 1, "0.07187500000000000138777878078144567552953..." },
    { 0x7FFFFFFFULL, 0, "5.789603771691175092122170506958809164960...e+76" },
    { 0x400001ULL, 0, "8.636170614117480412597919118381268959791...e-78" },
    { 0x0ULL, 0, "0" },
    { 0x80012345ULL, 0, "0" },
};

static unsigned check_texts(void)
{
    unsigned bad = 0;
    unsigned n = sizeof k_texts / sizeof k_texts[0];
    for (unsigned i = 0; i < n; i++) {
        char buf[72];
        size_t len = nd500_fx_text(k_texts[i].bits, k_texts[i].is_double != 0, buf, sizeof buf);
        if (len != strlen(k_texts[i].text) || strcmp(buf, k_texts[i].text) != 0) {
            printf("FAIL text %016llX: got \"%s\", expected \"%s\"\n",
                   (unsigned long long)k_texts[i].bits, buf, k_texts[i].text);
            bad++;
        }
    }
    printf("float_exact: %u of %u decimal texts correct\n", n - bad, n);
    return bad;
}

int main(void)
{
    unsigned bad_texts = check_texts();
    unsigned n = sizeof k_vectors / sizeof k_vectors[0];
    unsigned bad = 0;
    for (unsigned i = 0; i < n; i++) {
        unsigned exc = 0;
        uint64_t r;
        bool dbl = k_vectors[i].is_double != 0;
        switch (k_vectors[i].op) {
            case '+': r = nd500_fx_add(k_vectors[i].a, k_vectors[i].b, dbl, &exc); break;
            case '-': r = nd500_fx_sub(k_vectors[i].a, k_vectors[i].b, dbl, &exc); break;
            case '*': r = nd500_fx_mul(k_vectors[i].a, k_vectors[i].b, dbl, &exc); break;
            case 'r': r = nd500_fx_sqrt(k_vectors[i].a, dbl, &exc); break;
            default:  r = nd500_fx_div(k_vectors[i].a, k_vectors[i].b, dbl, &exc); break;
        }
        if (r != k_vectors[i].r || exc != k_vectors[i].exc) {
            if (bad < 10) {
                printf("FAIL %s %c %016llX %016llX: got %016llX exc %u, expected %016llX exc %u\n",
                       dbl ? "D" : "F", k_vectors[i].op,
                       (unsigned long long)k_vectors[i].a, (unsigned long long)k_vectors[i].b,
                       (unsigned long long)r, exc,
                       (unsigned long long)k_vectors[i].r, k_vectors[i].exc);
            }
            bad++;
        }
    }
    printf("float_exact: %u of %u vectors correct\n", n - bad, n);
    return bad == 0 && bad_texts == 0 ? 0 : 1;
}
