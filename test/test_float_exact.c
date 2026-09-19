/*
 * test_float_exact.c - src/cpu/float_exact.c against an independent reference
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * The vectors in float_exact_vectors.h come from tools/gen_float_exact_vectors.py,
 * which computes each result with exact rational arithmetic and rounds it by
 * ND-05.009.4 7.2.7, with the overflow and underflow results of 6.5.1.
 */

#include <stdio.h>
#include <stdint.h>
#include "float_exact.h"
#include "float_exact_vectors.h"

int main(void)
{
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
    return bad == 0 ? 0 : 1;
}
