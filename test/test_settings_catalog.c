/*
 * test_settings_catalog.c - lock the settings catalog so it cannot rot.
 *
 * WHY THIS TEST EXISTS
 * --------------------
 * The emulator's knobs used to be 108 getenv() calls spread over two dozen
 * files. Nothing described them, so they drifted: by the time they were
 * collected into Nd500Settings the catalog turned up
 *
 *   - two knobs set by tools/ndix/zrep.sh (ND500X_ZERO_PST, ND500X_UAREADBG)
 *     that NOTHING reads any more,
 *   - eight documented in markdown that no code reads,
 *   - four flags whose variable is INVERTED (the name says what it switches
 *     off), which is trivially easy to get backwards when adding a host,
 *   - and ND500X_TRAP_INVALID, which assigned 1 in both branches of its own
 *     if - so it has never had any effect at all.
 *
 * This test pins the parts a future change is most likely to break: the
 * documented defaults, and the direction of every inverted flag. The companion
 * CTest "settings_no_getenv" fails if a raw getenv reappears in the two core
 * libraries, which is what would let the catalog start drifting again.
 */

#include "nd500_settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(int ok, const char* what, long got, long want) {
    if (ok) {
        printf("  PASS  %s\n", what);
    } else {
        printf("  FAIL  %s (got %ld, want %ld)\n", what, got, want);
        failures++;
    }
}

#define EQ(expr, want, what) do { long g = (long)(expr); check(g == (long)(want), what, g, (long)(want)); } while (0)

/* Set a variable and reload, so each case starts from a known environment. */
static void set_and_load(const char* name, const char* value) {
    if (value) setenv(name, value, 1);
    else       unsetenv(name);
    nd500_settings_load_env();
}

int main(void) {
    printf("=== documented defaults (nd500_settings_defaults) ===\n");
    {
        Nd500Settings s;
        memset(&s, 0xAA, sizeof(s));      /* poison, so a missed field shows up */
        nd500_settings_defaults(&s);

        /* The fields whose documented default is NOT zero. Every one of these
         * is a trap for a host that fills the struct from scratch. */
        EQ(s.translate_bs,    1, "translate_bs defaults ON  (ND500X_RAW_BS is inverted)");
        EQ(s.tlb_enabled,     1, "tlb_enabled defaults ON   (ND500X_NOTLB is inverted)");
        EQ(s.demand_segments, 1, "demand_segments ON        (ND500X_NO_DEMAND_SEGMENTS inverted)");
        EQ(s.trap_dispatch,   1, "trap_dispatch ON          (ND500X_NO_TRAP_DISPATCH inverted)");
        EQ(s.show_hex,        1, "show_hex defaults ON      (only \"0\" turns it off)");
        EQ(s.trap_invalid,    1, "trap_invalid always ON");
        EQ(s.ptecatch,       -1, "ptecatch off is -1, not 0");
        EQ(s.pguwatch,       -1, "pguwatch off is -1, not 0");
        EQ(s.disk_mode,       1, "disk_mode defaults to write-through");

        /* And a representative sample of the zero-default diagnostics. */
        EQ(s.fedbg,   0, "fedbg defaults off");
        EQ(s.pgudbg,  0, "pgudbg defaults off");
        EQ(s.memtop,  0, "memtop 0 = report all memory");
        EQ(s.sfree,   0, "sfree 0 = use the built-in value");
    }

    printf("\n=== the loader maps each name to its field ===\n");
    set_and_load("ND500X_FEDBG", "1");
    EQ(nd500_settings()->fedbg, 1, "ND500X_FEDBG=1     -> fedbg");
    set_and_load("ND500X_FEDBG", "0");
    EQ(nd500_settings()->fedbg, 0, "ND500X_FEDBG=0     -> off");
    set_and_load("ND500X_FEDBG", NULL);
    EQ(nd500_settings()->fedbg, 0, "ND500X_FEDBG unset -> off");

    set_and_load("ND500X_MEMTOP", "0x800000");
    EQ(nd500_settings()->memtop, 0x800000, "ND500X_MEMTOP parses hex");
    set_and_load("ND500X_MEMTOP", "notanumber");
    EQ(nd500_settings()->memtop, 0, "ND500X_MEMTOP rejects a typo outright");
    set_and_load("ND500X_MEMTOP", NULL);

    printf("\n=== INVERTED flags point the right way ===\n");
    set_and_load("ND500X_NOTLB", "1");
    EQ(nd500_settings()->tlb_enabled, 0, "ND500X_NOTLB=1     -> tlb_enabled OFF");
    set_and_load("ND500X_NOTLB", NULL);
    EQ(nd500_settings()->tlb_enabled, 1, "ND500X_NOTLB unset -> tlb_enabled ON");

    set_and_load("ND500X_RAW_BS", "1");
    EQ(nd500_settings()->translate_bs, 0, "ND500X_RAW_BS=1     -> translate_bs OFF");
    set_and_load("ND500X_RAW_BS", NULL);
    EQ(nd500_settings()->translate_bs, 1, "ND500X_RAW_BS unset -> translate_bs ON");

    set_and_load("ND500X_NO_DEMAND_SEGMENTS", "1");
    EQ(nd500_settings()->demand_segments, 0, "ND500X_NO_DEMAND_SEGMENTS=1 -> OFF");
    set_and_load("ND500X_NO_DEMAND_SEGMENTS", NULL);

    set_and_load("ND500X_NO_TRAP_DISPATCH", "1");
    EQ(nd500_settings()->trap_dispatch, 0, "ND500X_NO_TRAP_DISPATCH=1   -> OFF");
    set_and_load("ND500X_NO_TRAP_DISPATCH", NULL);

    printf("\n=== the \"only literal 1\" flags stay strict ===\n");
    set_and_load("ND500X_TRACE", "1");
    EQ(nd500_settings()->trace, 1, "ND500X_TRACE=1   -> on");
    set_and_load("ND500X_TRACE", "yes");
    EQ(nd500_settings()->trace, 0, "ND500X_TRACE=yes -> OFF (only \"1\" counts)");
    set_and_load("ND500X_TRACE", NULL);

    set_and_load("ND500X_SHOW_HEX", "0");
    EQ(nd500_settings()->show_hex, 0, "ND500X_SHOW_HEX=0     -> off");
    set_and_load("ND500X_SHOW_HEX", NULL);
    EQ(nd500_settings()->show_hex, 1, "ND500X_SHOW_HEX unset -> ON by default");

    printf("\n=== ND500X_DISK_RW ===\n");
    set_and_load("ND500X_DISK_RW", NULL);
    EQ(nd500_settings()->disk_mode, 1, "unset -> 1 (write through)");
    set_and_load("ND500X_DISK_RW", "cow");
    EQ(nd500_settings()->disk_mode, 2, "\"cow\" -> 2 (session copy)");
    set_and_load("ND500X_DISK_RW", "0");
    EQ(nd500_settings()->disk_mode, 0, "\"0\"   -> 0 (read-only)");
    set_and_load("ND500X_DISK_RW", NULL);

    printf("\n=== ND500X_PWATCH: one variable, two fields ===\n");
    set_and_load("ND500X_PWATCH", "1A2B00");
    EQ(nd500_settings()->pwatch_base, 0x1A2B00, "base parses as hex");
    EQ(nd500_settings()->pwatch_len,  16,       "length defaults to 16");
    set_and_load("ND500X_PWATCH", "1000:64");
    EQ(nd500_settings()->pwatch_base, 0x1000, "base with an explicit length");
    EQ(nd500_settings()->pwatch_len,  64,     "explicit length is decimal");
    set_and_load("ND500X_PWATCH", NULL);
    EQ(nd500_settings()->pwatch_len, 0, "unset -> length 0 = watch nothing");

    printf("\n%s: %d failure(s)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
