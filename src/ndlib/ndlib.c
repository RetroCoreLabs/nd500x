/*
 * ndlib.c - logging helper
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "ndlib.h"

#include <stdio.h>
#include <stdarg.h>

/* When set (by the shell for a clean prompt), suppress informational logging. */
int nd500_log_quiet = 0;

void nd500_log(const char* fmt, ...) {
    if (nd500_log_quiet) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}


