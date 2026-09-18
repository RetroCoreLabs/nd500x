/*
 * ndlib_color.c - ANSI colour output for disassembly and debugging
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include "ndlib_color.h"
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

/* Global color state - managed by ndlib_color_init() */
static bool use_color = false;

void ndlib_color_init(int ansi_flag) {
    if (ansi_flag == 1) {
        /* Force enable via -ansi flag */
        use_color = true;
        return;
    }
    if (ansi_flag == -1) {
        /* Force disable via -noansi flag */
        use_color = false;
        return;
    }

    /* Auto-detect: check if stdout is a TTY and terminal supports ANSI */
    const char *term = getenv("TERM");
    bool term_supports_ansi = false;

    if (term) {
        /* Check for common terminal types that support ANSI escape codes */
        if (strstr(term, "xterm") ||
            strstr(term, "ansi") ||
            strstr(term, "vt100") ||
            strstr(term, "rxvt") ||
            strstr(term, "screen") ||
            strstr(term, "tmux") ||
            strstr(term, "linux") ||
            strstr(term, "color")) {
            term_supports_ansi = true;
        }
    }

    /* Enable color only if both TTY and ANSI-capable terminal */
    use_color = isatty(STDOUT_FILENO) && term_supports_ansi;
}

/* Color accessor functions - return ANSI codes or empty strings */

const char* color_address(void) {
    return use_color ? "\033[90m" : "";
}

const char* color_bytes(void) {
    return use_color ? "\033[33m" : "";
}

const char* color_instr(void) {
    return use_color ? "\033[92m" : "";
}

const char* color_oper(void) {
    return use_color ? "\033[97m" : "";
}

const char* color_branch(void) {
    return use_color ? "\033[91m" : "";
}

const char* color_label(void) {
    return use_color ? "\033[96m" : "";
}

const char* color_comment(void) {
    return use_color ? "\033[94m" : "";
}

const char* color_meta(void) {
    return use_color ? "\033[93m" : "";
}

const char* color_reset(void) {
    return use_color ? "\033[0m" : "";
}

int ndlib_color_enabled(void) {
    return use_color ? 1 : 0;
}

