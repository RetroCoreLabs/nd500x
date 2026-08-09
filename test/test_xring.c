/*
 * XMSG ring-discipline tests.
 *
 * These rules cannot be checked by booting: every way of getting them wrong
 * presents as "nothing happened" inside a running guest - a ring that reads as
 * empty, a kick that never comes again. So they are checked here, against a
 * plain byte array standing in for the segment-6 window, with NDIX's own
 * arithmetic (if/xg.c R_init, R_put, R_get) as the answer key.
 *
 * The one that earns its keep is FULL. NDIX computes it as
 *
 *     (p == k - 1) || ((k == 0) && (p == mp - 2))
 *
 * which wastes a slot and uses mp - 2 where an ordinary ring buffer would use
 * mp - 1. Anyone tidying that into the obvious form makes this side fill one
 * slot further than NDIX expects, and NDIX reads the ring as EMPTY.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/cpu/nd500_xring.h"

static int passed = 0, failed = 0;

static void check(const char* what, int ok) {
    if (ok) { passed++; printf("  [PASS] %s\n", what); }
    else    { failed++; printf("  [FAIL] %s\n", what); }
}
static void check_eq(const char* what, long expect, long got) {
    if (expect == got) { passed++; printf("  [PASS] %s\n", what); }
    else { failed++; printf("  [FAIL] %s: expected %ld, got %ld\n", what, expect, got); }
}

/* The segment-6 window as a flat array. Only 0x30000000..0x30000FFF matters,
 * so the array is that window and the accessor subtracts the base. */
#define WIN_BASE 0x30000000u
#define WIN_LEN  0x1000u
static uint8_t g_win[WIN_LEN];

static uint8_t win_read8(void* ctx, uint32_t a) {
    (void)ctx;
    if (a < WIN_BASE || a >= WIN_BASE + WIN_LEN) return 0;
    return g_win[a - WIN_BASE];
}
static void win_write8(void* ctx, uint32_t a, uint8_t v) {
    (void)ctx;
    if (a < WIN_BASE || a >= WIN_BASE + WIN_LEN) return;
    g_win[a - WIN_BASE] = v;
}
static Nd500XRingMem g_mem = { win_read8, win_write8, NULL };

int main(void) {
    Nd500XRing cmd, resp;
    uint8_t entry[XRING_CMD_SIZE], got[XRING_CMD_SIZE];
    int i;

    printf("XMSG ring discipline\n");
    memset(g_win, 0xEE, sizeof g_win);      /* not zero, so "unwritten" shows */

    nd500_xring_cmd_init(&cmd);
    nd500_xring_resp_init(&resp);

    /* ---- geometry, against the numbers NDIX compiles in ---- */
    check_eq("command entries", 102, cmd.entries);
    check_eq("response entries", 113, resp.entries);
    check_eq("command entry size", 20, cmd.entry_size);
    check_eq("response entry size", 18, resp.entry_size);
    /* Both rings must fit their 0x800 half of the window, header included. */
    check("102 commands fit in 0x800",
          XRING_HDR_SIZE + 102 * 20 <= 0x800);
    check("113 responses fit in 0x800",
          XRING_HDR_SIZE + 113 * 18 <= 0x800);
    check("103 commands would NOT fit (102 is the real maximum)",
          XRING_HDR_SIZE + 103 * 20 > 0x800);
    check("114 responses would NOT fit",
          XRING_HDR_SIZE + 114 * 18 > 0x800);
    check_eq("command ring base", 0x30000000L, (long)cmd.base);
    check_eq("response ring base", 0x30000800L, (long)resp.base);
    check_eq("entry 0 starts after the 6-byte header",
             0x30000006L, (long)nd500_xring_entry_addr(&cmd, 0));
    check_eq("entry 1 is 20 bytes further on",
             0x3000001AL, (long)nd500_xring_entry_addr(&cmd, 1));

    /* ---- the headers R_init() demands, or it panics ---- */
    nd500_xring_write_headers(&g_mem, &cmd);
    nd500_xring_write_headers(&g_mem, &resp);
    check_eq("cmd p", 0, nd500_xring_p(&g_mem, &cmd));
    check_eq("cmd k", 0, nd500_xring_k(&g_mem, &cmd));
    check_eq("cmd mp is 102, which R_init checks for", 102, nd500_xring_mp(&g_mem, &cmd));
    check_eq("resp mp is 113", 113, nd500_xring_mp(&g_mem, &resp));
    /* mp is big-endian in guest memory: 102 = 0x0066. Byte order matters here
     * because the ND-500 reads these as shorts, not as our uint16_t. */
    check_eq("mp is stored big-endian (high byte)", 0x00, g_win[4]);
    check_eq("mp is stored big-endian (low byte)",  0x66, g_win[5]);

    /* ---- empty and full ---- */
    check("a fresh ring is empty", nd500_xring_empty(&g_mem, &cmd));
    check("a fresh ring is not full", !nd500_xring_full(&g_mem, &cmd));

    /* ---- one round trip ---- */
    for (i = 0; i < XRING_CMD_SIZE; i++) entry[i] = (uint8_t)(0x10 + i);
    check("put one entry", nd500_xring_put(&g_mem, &cmd, entry) == 1);
    check("the ring is no longer empty", !nd500_xring_empty(&g_mem, &cmd));
    check_eq("p advanced", 1, nd500_xring_p(&g_mem, &cmd));
    check_eq("k did not", 0, nd500_xring_k(&g_mem, &cmd));
    memset(got, 0, sizeof got);
    check("get it back", nd500_xring_get(&g_mem, &cmd, got) == 1);
    check("the bytes are the ones that went in", memcmp(entry, got, XRING_CMD_SIZE) == 0);
    check_eq("k advanced", 1, nd500_xring_k(&g_mem, &cmd));
    check("and the ring is empty again", nd500_xring_empty(&g_mem, &cmd));
    check("getting from an empty ring returns 0", nd500_xring_get(&g_mem, &cmd, got) == 0);

    /* ---- the entry landed where NDIX will look for it ---- */
    check_eq("entry 0 byte 0 is at base+6", 0x10, g_win[6]);
    check_eq("entry 0 byte 19 is at base+25", 0x10 + 19, g_win[25]);

    /* ---- wrap ----
     * Modulo the compile-time 102, not modulo mp. They agree here because
     * R_init insists they do, but the code must not depend on that. */
    nd500_xring_write_headers(&g_mem, &cmd);
    nd500_xring_set_p(&g_mem, &cmd, 101);
    nd500_xring_set_k(&g_mem, &cmd, 101);
    check("p == k == 101 is empty", nd500_xring_empty(&g_mem, &cmd));
    check("put at the last slot", nd500_xring_put(&g_mem, &cmd, entry) == 1);
    check_eq("p wrapped to 0", 0, nd500_xring_p(&g_mem, &cmd));
    check("get wraps too", nd500_xring_get(&g_mem, &cmd, got) == 1);
    check_eq("k wrapped to 0", 0, nd500_xring_k(&g_mem, &cmd));

    /* ---- FULL, arm one: p == k - 1 ---- */
    nd500_xring_write_headers(&g_mem, &cmd);
    nd500_xring_set_k(&g_mem, &cmd, 50);
    nd500_xring_set_p(&g_mem, &cmd, 49);
    check("p == k-1 is FULL", nd500_xring_full(&g_mem, &cmd));
    check("and a put is refused", nd500_xring_put(&g_mem, &cmd, entry) == 0);
    check_eq("a refused put does not move p", 49, nd500_xring_p(&g_mem, &cmd));
    nd500_xring_set_p(&g_mem, &cmd, 48);
    check("p == k-2 is not full", !nd500_xring_full(&g_mem, &cmd));

    /* ---- FULL, arm two: the one that looks like a bug ----
     * k == 0 and p == mp - 2 = 100. NOT 101. A ring that allowed 101 would put
     * one entry more than NDIX will ever read. */
    nd500_xring_write_headers(&g_mem, &cmd);
    nd500_xring_set_k(&g_mem, &cmd, 0);
    nd500_xring_set_p(&g_mem, &cmd, 100);
    check("k==0, p==mp-2 (100) is FULL - the asymmetric arm", nd500_xring_full(&g_mem, &cmd));
    check("a put there is refused", nd500_xring_put(&g_mem, &cmd, entry) == 0);
    nd500_xring_set_p(&g_mem, &cmd, 99);
    check("k==0, p==99 is not full", !nd500_xring_full(&g_mem, &cmd));
    /* p == 101 with k == 0 is NOT full by NDIX'''s rule, and that is not a defect
     * to paper over. Arm one wants p == -1 and arm two wants p == 100, so 101
     * matches neither. It is also UNREACHABLE from a correct producer: filling
     * from empty with k == 0 stops at p == 100, because that is where FULL
     * bites. Asserting "101 must be full too" was this test'''s own invention -
     * it failed, correctly, and the requirement was mine rather than NDIX'''s.
     * What matters is that the rule is copied, not that it is tidy. */
    nd500_xring_set_p(&g_mem, &cmd, 101);
    check("k==0, p==101 is not full - NDIX'''s rule says so, and a correct "
          "producer never gets there", !nd500_xring_full(&g_mem, &cmd));

    /* ---- k == 0 must not make arm one match by accident ----
     * k - 1 is -1 in NDIX's signed short arithmetic, so p can never equal it.
     * Computing it unsigned here would make it 65535 and the arm would be dead
     * in a different way; either mistake changes when the ring reports full. */
    nd500_xring_write_headers(&g_mem, &cmd);
    nd500_xring_set_k(&g_mem, &cmd, 0);
    nd500_xring_set_p(&g_mem, &cmd, 1);
    check("k==0, p==1 is not full", !nd500_xring_full(&g_mem, &cmd));
    check("and not empty", !nd500_xring_empty(&g_mem, &cmd));

    /* ---- fill it the way a real producer would ----
     * From empty, how many entries actually go in before FULL bites? With
     * k == 0 the answer is mp - 2 = 100, one fewer than the slots suggest.
     * Pinning the number down is what stops a later "optimisation". */
    nd500_xring_write_headers(&g_mem, &cmd);
    {
        int n = 0;
        while (nd500_xring_put(&g_mem, &cmd, entry)) n++;
        check_eq("entries accepted from empty with k==0", 100, n);
        check("the ring reports full", nd500_xring_full(&g_mem, &cmd));
        check("and it is not empty", !nd500_xring_empty(&g_mem, &cmd));
        /* Drain it all back out, in order. */
        n = 0;
        while (nd500_xring_get(&g_mem, &cmd, got)) n++;
        check_eq("and all of them come back", 100, n);
        check("empty again", nd500_xring_empty(&g_mem, &cmd));
    }

    /* ---- FIFO order over a wrap, on the response ring ---- */
    nd500_xring_write_headers(&g_mem, &resp);
    {
        uint8_t e[XRING_RESP_SIZE], g[XRING_RESP_SIZE];
        int seq, out_seq = 0, bad = 0;
        nd500_xring_set_p(&g_mem, &resp, 110);
        nd500_xring_set_k(&g_mem, &resp, 110);
        for (seq = 0; seq < 8; seq++) {           /* crosses 113 -> 0 */
            memset(e, 0, sizeof e);
            nd500_xring_put_be16(e, (uint16_t)(0x1000 + seq));
            if (!nd500_xring_put(&g_mem, &resp, e)) { bad = 1; break; }
        }
        check("eight responses across the wrap", !bad);
        while (nd500_xring_get(&g_mem, &resp, g)) {
            if (nd500_xring_be16(g) != (uint16_t)(0x1000 + out_seq)) bad = 1;
            out_seq++;
        }
        check_eq("all eight came back", 8, out_seq);
        check("in the order they went in", !bad);
    }

    /* ---- big-endian helpers ---- */
    {
        uint8_t b[4];
        nd500_xring_put_be16(b, 0x1234);
        check_eq("be16 high byte first", 0x12, b[0]);
        check_eq("be16 low byte second", 0x34, b[1]);
        check_eq("be16 reads back", 0x1234, nd500_xring_be16(b));
        nd500_xring_put_be32(b, 0x89ABCDEFu);
        check_eq("be32 byte 0", 0x89, b[0]);
        check_eq("be32 byte 3", 0xEF, b[3]);
        check_eq("be32 reads back", (long)0x89ABCDEFu, (long)nd500_xring_be32(b));
    }

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
