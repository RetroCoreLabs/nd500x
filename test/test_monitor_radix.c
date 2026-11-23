/*
 * test_monitor_radix.c - the radix the debugger monitor prints numbers in.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Ronny Hansen
 *
 * The monitor commands regs, set, m, mp, m! and status print whole numbers in the radix of
 * nd500_dbg_get_show_radix(): octal by default (177773B, as the ND tools print numbers),
 * decimal or hex (0FFFBH) after --radix or "set radix". The double registers D1-D4 are the
 * exact decimal of E:A (nd500_fx_text), the same text as the DAP Float scope.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/machine/machine_protos.h"
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/float_exact.h"
#include "../src/debugger/commands.h"

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        g_tests_run++;                                                         \
        if (!(cond)) {                                                         \
            g_tests_failed++;                                                  \
            printf("FAIL: %s (line %d): %s\n", __func__, __LINE__, msg);       \
        }                                                                      \
    } while (0)

static Nd500Machine g_m;
static Nd500Cpu g_cpu;
static char g_out[16384];

static void collect(const char* line, void* ctx) {
    (void)ctx;
    size_t n = strlen(g_out);
    snprintf(g_out + n, sizeof g_out - n, "%s\n", line);
}

/* Run one monitor command; its output (and errors) are in g_out */
static int run(const char* cmd) {
    CmdContext ctx = {collect, collect, NULL};
    char line[256];
    g_out[0] = '\0';
    snprintf(line, sizeof line, "%s", cmd);
    return nd500_cmd_execute(&g_m, line, &ctx);
}

static int has(const char* text) {
    int ok = strstr(g_out, text) != NULL;
    if (!ok) printf("  wanted \"%s\" in:\n%s\n", text, g_out);
    return ok;
}

static void test_shared_text(void) {
    char t[24];
    nd500_num_text(t, sizeof t, 0xFFFFFFFBu, 32, false, 2);
    CHECK(strcmp(t, "37777777773B") == 0, "octal, B suffix, no leading zeros");
    nd500_num_text(t, sizeof t, 0xFFFFFFFBu, 32, true, 0);
    CHECK(strcmp(t, "-5") == 0, "decimal, signed");
    nd500_num_text(t, sizeof t, 0xFFFFFFFBu, 32, false, 0);
    CHECK(strcmp(t, "4294967291") == 0, "decimal, unsigned");
    nd500_num_text(t, sizeof t, 0xFFFBu, 16, false, 1);
    CHECK(strcmp(t, "0FFFBH") == 0, "hex, 0 before a letter, H suffix");
    nd500_num_text(t, sizeof t, 0x1234u, 32, false, 1);
    CHECK(strcmp(t, "1234H") == 0, "hex without a letter first");
}

static void test_regs(void) {
    g_cpu.I[0] = 0xDEADBEEFu;
    g_cpu.PC = 0x08023E9Cu;
    g_cpu.E[0] = 0x3F499999u;
    g_cpu.A[0] = 0x9999999Au;
    CHECK(nd500_dbg_get_show_radix() == 2, "octal is the default");
    CHECK(run("regs") == 0, "regs");
    CHECK(has("  PC                   = 1000437234B "), "PC in octal");
    CHECK(has("  I1                   = 33653337357B "), "I1 in octal");
    /* D1: the exact value of E1:A1, worked out with exact rational arithmetic in
     * test_float_exact.c; a host %f gave 0.071875 */
    CHECK(has("  D1                   = 0.07187500000000000138777878078144567552953...\n"),
          "D1 is the exact value of E1:A1");
    CHECK(has("  E1                   = 7722314631B "), "E1 in octal (0x3F499999)");

    CHECK(run("set radix hex") == 0 && has("radix: hex"), "set radix hex");
    run("regs");
    CHECK(has("  PC                   = 8023E9CH "), "PC in hex");
    CHECK(has("  I1                   = 0DEADBEEFH "), "I1 in hex, 0 before a letter");
    CHECK(run("set radix decimal") == 0, "set radix decimal");
    run("regs");
    CHECK(has("  I1                   = 3735928559 "), "I1 in decimal (a register is unsigned)");
    nd500_dbg_set_radix(2);
}

static void test_set_echo(void) {
    CHECK(run("set I2 0x12345678") == 0, "set I2");
    CHECK(g_cpu.I[1] == 0x12345678u, "I2 written");
    CHECK(has("I2 = 2215053170B"), "the new value echoed in octal");
}

static void test_memory_dump(void) {
    memcpy(g_m.memory + 0x4000, "HELLO\xff", 6);
    CHECK(run("m! 0x4000 6") == 0, "m! dump");
    CHECK(has("40000B: 110 105 114 114 117 377 "), "address and bytes in octal");
    CHECK(has("|HELLO.          |"), "ASCII column");
    nd500_dbg_set_radix(1);
    run("m! 0x4000 6");
    CHECK(has("4000H: 48 45 4C 4C 4F FF "), "address and bytes in hex");
    nd500_dbg_set_radix(0);
    run("m! 16384 6");
    CHECK(has("16384:  72  69  76  76  79 255 "), "address and bytes in decimal");
    nd500_dbg_set_radix(2);
}

static void test_status(void) {
    g_cpu.PC = 0x1000u;
    g_m.stop_reason = STOP_NONE;
    g_m.run_flag = 0;
    CHECK(run("status") == 0, "status");
    CHECK(has("PC=10000B"), "status prints PC in octal");
}

int main(void) {
    printf("Monitor radix tests\n");
    nd500_machine_init(&g_m, 1024 * 1024);
    nd500_cpu_init(&g_cpu, &g_m);
    nd500_cpu_reset(&g_cpu);
    g_m.cpu = &g_cpu;

    test_shared_text();
    test_regs();
    test_set_echo();
    test_memory_dump();
    test_status();

    printf("%d checks, %d failures\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
