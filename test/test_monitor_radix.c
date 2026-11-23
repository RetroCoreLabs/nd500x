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
 *
 * Numbers TYPED into the monitor are read in the same radix (octal by default) or in an
 * explicit form (17B, 15D, 0FH, 0x0F, $15); the disassembly operands, bp, wp, stackframe and
 * showmmu print in the radix too.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/machine/machine_protos.h"
#include "../src/cpu/cpu_protos.h"
#include "../src/cpu/float_exact.h"
#include "../src/debugger/commands.h"
#include "../src/disasm/nd500_disasm.h"
#include <unistd.h>

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

/* Numbers typed into the monitor: bare in the radix (octal by default), and the explicit
 * forms of the ND-500 Loader Monitor manual (ND-60.136.04A: B octal, D decimal, H hex) plus
 * the earlier nd500x forms 0x.., $.. and a leading zero */
static uint32_t num(const char* text, int* ok) {
    uint32_t v = 0xDEADDEADu;
    *ok = nd500_cmd_parse_number(text, &v) == 0;
    return v;
}

static void test_typed_numbers(void) {
    int ok = 0;
    CHECK(nd500_dbg_get_radix() == 2, "typed numbers are octal by default");
    CHECK(num("17", &ok) == 15u && ok, "bare 17 in the octal radix is 15");
    CHECK(num("19", &ok), "19 in the octal radix");
    CHECK(!ok, "19 is not an octal number");
    CHECK(nd500_cmd_parse_u32("19", 7u) == 7u, "parse_u32 gives the default for 19 in octal");
    CHECK(num("17B", &ok) == 15u && ok, "17B is octal");
    CHECK(num("17D", &ok) == 17u && ok, "17D is decimal");
    CHECK(num("17H", &ok) == 23u && ok, "17H is hex");
    CHECK(num("0FH", &ok) == 15u && ok, "0FH is hex");
    CHECK(num("0ffh", &ok) == 255u && ok, "suffix and digits in lower case");
    CHECK(num("0x1F", &ok) == 31u && ok, "0x is hex");
    CHECK(num("$19", &ok) == 19u && ok, "$ is decimal");
    CHECK(num("-1", &ok) == 0xFFFFFFFFu && ok, "-1 is the two's complement");
    CHECK(num("FH", &ok), "FH");
    CHECK(!ok, "a hex number must start with a digit (FH is a name)");
    CHECK(num("", &ok), "empty");
    CHECK(!ok, "empty text is not a number");
    CHECK(num("37777777777B", &ok) == 0xFFFFFFFFu && ok, "the largest 32-bit octal");
    CHECK(num("40000000000B", &ok), "33 bits");
    CHECK(!ok, "more than 32 bits is not a number");

    nd500_dbg_set_radix(0);
    CHECK(num("17", &ok) == 17u && ok, "bare 17 in the decimal radix");
    CHECK(num("017", &ok) == 15u && ok, "a leading zero is octal in the decimal radix");
    CHECK(num("17B", &ok) == 15u && ok, "17B in the decimal radix");
    CHECK(num("1F", &ok), "1F in decimal");
    CHECK(!ok, "1F is not a decimal number");

    nd500_dbg_set_radix(1);
    CHECK(num("17", &ok) == 23u && ok, "bare 17 in the hex radix");
    CHECK(num("017", &ok) == 23u && ok, "a leading zero is a redundant hex zero in the hex radix");
    CHECK(num("FF", &ok) == 255u && ok, "FF in the hex radix");
    CHECK(num("1B", &ok) == 1u && ok, "1B is octal 1 in every radix (the suffix is read first)");
    CHECK(num("1BH", &ok) == 27u && ok, "1BH is hex 1B");
    CHECK(num("1D", &ok) == 1u && ok, "1D is decimal 1 in every radix");
    nd500_dbg_set_radix(2);

    /* a command reads its numbers that way: m! 40000 = 0x4000 in octal */
    memcpy(g_m.memory + 0x4000, "AB", 2);
    CHECK(run("m! 40000 2") == 0 && has("40000B: 101 102 "), "m! 40000 reads octal 40000");
    CHECK(run("m! 16384D 2D") == 0 && has("40000B: 101 102 "), "m! 16384D 2D");
    CHECK(run("m! 4000H 2") == 0 && has("40000B: 101 102 "), "m! 4000H");
}

/* "dis": the operand numbers in the radix. W STZ b.172 (bytes 4A C1 AC, as in
 * test_unsigned_displacement.c): 172 = 254B = 0ACH */
static void test_disassembly(void) {
    static const uint8_t code[] = {0x4A, 0xC1, 0xAC};
    char buf[1024];
    memcpy(g_m.memory + 0x5000, code, sizeof code);
    nd500_disasm_format_range(&g_m, 0x5000, 3, buf, sizeof buf);
    CHECK(strstr(buf, "b.254B") != NULL, "displacement in octal by default");
    if (!strstr(buf, "b.254B")) printf("  got: %s\n", buf);
    nd500_dbg_set_radix(0);
    nd500_disasm_format_range(&g_m, 0x5000, 3, buf, sizeof buf);
    CHECK(strstr(buf, "b.172") != NULL && strstr(buf, "b.172B") == NULL, "displacement in decimal");
    nd500_dbg_set_radix(1);
    nd500_disasm_format_range(&g_m, 0x5000, 3, buf, sizeof buf);
    CHECK(strstr(buf, "b.0ACH") != NULL, "displacement in hex");
    nd500_dbg_set_radix(2);
    /* a caller with a radix of its own (the DAP session) */
    nd500_disasm_set_radix(0);
    nd500_disasm_format_range(&g_m, 0x5000, 3, buf, sizeof buf);
    CHECK(strstr(buf, "b.172") != NULL && strstr(buf, "b.172B") == NULL, "own radix: decimal");
    nd500_disasm_set_radix(-1);
    nd500_disasm_format_range(&g_m, 0x5000, 3, buf, sizeof buf);
    CHECK(strstr(buf, "b.254B") != NULL, "back to the monitor's radix");
}

/* stackframe and showmmu print in the radix */
static void test_stackframe_showmmu(void) {
    g_cpu.B = 0x6000u;
    memset(g_m.memory + 0x6000, 0, 32);
    g_m.memory[0x6004 + 3] = 0x10; /* RETA = 0x10 (big-endian word) */
    CHECK(run("stackframe") == 0, "stackframe");
    CHECK(has("Stack Frame at B=60000B:"), "B in octal");
    CHECK(has("RETA  = 20B "), "RETA in octal");
    CHECK(has("+10B  SP "), "offset 8 in octal");
    nd500_dbg_set_radix(0);
    run("stackframe");
    CHECK(has("Stack Frame at B=24576:"), "B in decimal");
    CHECK(has("+8    SP "), "offset 8 in decimal");
    nd500_dbg_set_radix(1);
    run("stackframe");
    CHECK(has("Stack Frame at B=6000H:"), "B in hex");
    nd500_dbg_set_radix(2);

    g_cpu.PSTP = 0x80000u;
    CHECK(run("showmmu") == 0, "showmmu");
    CHECK(has("PSTP    = 2000000B "), "PSTP in octal");
    nd500_dbg_set_radix(1);
    run("showmmu");
    CHECK(has("PSTP    = 80000H "), "PSTP in hex");
    nd500_dbg_set_radix(0);
    run("showmmu");
    CHECK(has("PSTP    = 524288 "), "PSTP in decimal");
    CHECK(has("PST: "), "the counts stay (the web debugger reads them)");
    nd500_dbg_set_radix(2);
}

/* bp and wp print to stderr: catch it in a file */
static int err_has(const char* path, const char* text) {
    FILE* f = fopen(path, "r");
    char all[8192];
    size_t n = f ? fread(all, 1, sizeof all - 1, f) : 0;
    if (f) fclose(f);
    all[n] = '\0';
    int ok = strstr(all, text) != NULL;
    if (!ok) printf("  wanted \"%s\" in stderr:\n%s\n", text, all);
    return ok;
}

static void test_bp_wp(void) {
    char path[] = "/tmp/test_monitor_radix_XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0, "temporary file");
    if (fd < 0) return;
    fflush(stderr);
    int saved = dup(2);
    dup2(fd, 2);
    run("bp 1000");
    run("bp list");
    run("wp 2000H 10 write");
    run("wp list");
    nd500_dbg_set_radix(0);
    run("bp list");
    nd500_dbg_set_radix(2);
    fflush(stderr);
    dup2(saved, 2);
    close(saved);
    close(fd);
    CHECK(err_has(path, "set at 1000B"), "bp 1000 is octal 1000, echoed in octal");
    CHECK(err_has(path, "  0 1000B "), "bp list in octal");
    CHECK(err_has(path, "set at 20000B (length=10B"), "wp 2000H 10: hex address, octal length");
    CHECK(err_has(path, "  0 512 "), "bp list in decimal");
    remove(path);
    run("bp del 0");
    run("wp del 0");
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
    test_typed_numbers();
    test_disassembly();
    test_stackframe_showmmu();
    test_bp_wp();

    printf("%d checks, %d failures\n", g_tests_run, g_tests_failed);
    return g_tests_failed == 0 ? 0 : 1;
}
