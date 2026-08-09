/*
 * XMSG server tests - what the ND-100 side answers, and how.
 *
 * test_xring.c checks the ring ARITHMETIC. This file checks the ANSWERS, and
 * the reason it exists is the wedge: xgintr() (NDIX if/xg.c:374) picks which
 * outstanding-request flag to clear from the RESPONSE's own func field. Send
 * back a func that does not match the command it answers and the wrong flag is
 * cleared, the real request is never retired, every later xgdctl() on that
 * sub-device returns EBUSY, and nothing at all is printed. There is no way to
 * see that from a booting guest, so it is pinned here.
 *
 * The other thing pinned here is "always answer". A command left unanswered
 * wedges its sub-device exactly the same way, so an unknown function must come
 * back as an ERROR rather than be dropped.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "../src/cpu/nd500_xring.h"
#include "../src/cpu/nd500_xmsg.h"

static int passed = 0, failed = 0;

static void check(const char* what, int ok) {
    if (ok) { passed++; printf("  [PASS] %s\n", what); }
    else    { failed++; printf("  [FAIL] %s\n", what); }
}
static void check_eq(const char* what, long expect, long got) {
    if (expect == got) { passed++; printf("  [PASS] %s\n", what); }
    else { failed++; printf("  [FAIL] %s: expected %ld, got %ld\n", what, expect, got); }
}

/* The segment-6 window as a flat array, same stand-in as test_xring.c. */
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

/* Command/response field offsets, repeated here on purpose: the test should
 * fail if nd500_xmsg.c's private copies ever drift from if/xmsg.h. */
#define CMD_SEQWORD 0
#define CMD_SUBDEV  4
#define CMD_FUNC    6
#define CMD_ARG_T   8
#define CMD_MAGNO  16
#define RESP_SEQWORD 0
#define RESP_SUBDEV  4
#define RESP_FUNC    6
#define RESP_ARG_T   8
#define RESP_ARG_A  10
#define RESP_CBA    16

static Nd500XRing g_cmd, g_resp;

/* Put one command in the ring exactly as NDIX's R_put would (if/xg.c:471-480):
 * seq word, sub-device, func, then the four argument registers, then magno. */
static void put_cmd(uint32_t seqword, uint16_t subdev, uint16_t func) {
    uint8_t e[XRING_CMD_SIZE];
    memset(e, 0, sizeof e);
    nd500_xring_put_be32(e + CMD_SEQWORD, seqword);
    nd500_xring_put_be16(e + CMD_SUBDEV, subdev);
    nd500_xring_put_be16(e + CMD_FUNC, func);
    nd500_xring_put_be16(e + CMD_ARG_T, func);   /* xp->args = *ap, so T == func */
    nd500_xring_put_be32(e + CMD_MAGNO, 0);
    check("command accepted into the ring", nd500_xring_put(&g_mem, &g_cmd, e) == 1);
}

/* Take the next response out, the way xgintr()/R_get would. */
static int get_resp(uint8_t* out) {
    return nd500_xring_get(&g_mem, &g_resp, out);
}

static void reset_window(void) {
    memset(g_win, 0, sizeof g_win);
    nd500_xring_cmd_init(&g_cmd);
    nd500_xring_resp_init(&g_resp);
    nd500_xring_write_headers(&g_mem, &g_cmd);
    nd500_xring_write_headers(&g_mem, &g_resp);
    nd500_xmsg_reset();
}

int main(void) {
    uint8_t r[XRING_RESP_SIZE];

    printf("XMSG server\n");

    /* ---- 1. XFOPN is answered, with a usable port number ------------------
     * if/if_et.c:175 issues XFOPN during etattach() and, on success, keeps
     * `es->es_portno = xa->A`. It treats ANY negative T as a failure
     * ("bad XFOPN") and gives up on the interface, so T must be XMSUX = 0. */
    printf("\nXFOPN\n");
    reset_window();
    put_cmd(0x00000000u, 0, XMSG_XFOPN);
    check_eq("one command answered", 1, nd500_xmsg_service_mem(&g_mem));
    check("a response is waiting", get_resp(r) == 1);
    check_eq("T is XMSUX (success)", 0, (int16_t)nd500_xring_be16(r + RESP_ARG_T));
    check("A is a non-zero port number", nd500_xring_be16(r + RESP_ARG_A) != 0);
    check_eq("cba is 0", 0, nd500_xring_be16(r + RESP_CBA));
    check("command ring is drained", nd500_xring_empty(&g_mem, &g_cmd) == 1);
    check("response ring is now empty again", nd500_xring_empty(&g_mem, &g_resp) == 1);

    /* ---- 2. THE WEDGE: seq, subdev and func come back verbatim ------------
     * xgintr() reads the RESPONSE's func to decide whether to clear HAS_RCV or
     * HAS_OTHER (xg.c:374 is_receive(rp->func)), and its DEBUG_XMSG build even
     * panics "WRONG RESPONSE" when a response's seq matches a command with a
     * different subdev (xg.c:527-538). Getting any of the three wrong is
     * silent in a normal build. */
    printf("\nseq/subdev/func are echoed exactly\n");
    reset_window();
    put_cmd(0x2468ACE0u, 3, XMSG_XFOPN);
    check_eq("answered", 1, nd500_xmsg_service_mem(&g_mem));
    check(" response present", get_resp(r) == 1);
    check_eq("seq word echoed", (long)0x2468ACE0u,
             (long)nd500_xring_be32(r + RESP_SEQWORD));
    check_eq("subdev echoed", 3, nd500_xring_be16(r + RESP_SUBDEV));
    check_eq("func echoed", XMSG_XFOPN, nd500_xring_be16(r + RESP_FUNC));

    /* The option bits above the low byte travel with the func and must survive
     * the round trip too: if_et.c sends XFRREN|XFWAK|XFRMR (if_et.c:486), and
     * is_receive() masks with XFMASK itself. Stripping the options here would
     * hand xgintr() a func NDIX never sent. */
    reset_window();
    put_cmd(1u << 1, 0, (uint16_t)(060 | 0x8000 | 0x1000));   /* XFRREN|XFWAK|XFRMR */
    check_eq("answered", 1, nd500_xmsg_service_mem(&g_mem));
    check(" response present", get_resp(r) == 1);
    check_eq("func echoed WITH its option bits", (long)(060 | 0x8000 | 0x1000),
             (long)nd500_xring_be16(r + RESP_FUNC));

    /* ---- 3. An unknown function is answered, not dropped ------------------ */
    printf("\nunknown functions still get an answer\n");
    reset_window();
    put_cmd(4u << 1, 0, 055 /* XETHER - not implemented yet */);
    check_eq("answered", 1, nd500_xmsg_service_mem(&g_mem));
    check(" response present", get_resp(r) == 1);
    check("T is negative (an error NDIX can print)",
          (int16_t)nd500_xring_be16(r + RESP_ARG_T) < 0);
    check_eq("T is XENIM", XMSG_XENIM, (int16_t)nd500_xring_be16(r + RESP_ARG_T));
    check_eq("func still echoed", 055, nd500_xring_be16(r + RESP_FUNC));

    /* ---- 4. DRAIN TO EMPTY -----------------------------------------------
     * R_put issues DCTL_KICK only on the empty -> non-empty transition
     * (xg.c:484). Stop with entries still in the ring and no kick ever comes
     * again: the link stalls in silence. So one service call must clear
     * everything that is in there, however much that is. */
    printf("\none service call drains the whole ring\n");
    reset_window();
    {
        int i;
        for (i = 0; i < 40; i++) put_cmd((uint32_t)i << 1, (uint16_t)(i % 8), XMSG_XFOPN);
        check_eq("all 40 answered in one call", 40, nd500_xmsg_service_mem(&g_mem));
        check("command ring empty", nd500_xring_empty(&g_mem, &g_cmd) == 1);
        for (i = 0; i < 40; i++) {
            if (!get_resp(r)) { check("40 responses available", 0); break; }
        }
        check("exactly 40 responses, no more", get_resp(r) == 0);
    }

    /* ---- 5. One port per sub-device, distinct across sub-devices ----------
     * "at most 1 Xmsg port open on a channel" (if_et.c:141). Re-opening the
     * same sub-device must return the SAME port - handing out a fresh number
     * would leave es_portno pointing at a port the previous open still owns. */
    printf("\nport allocation\n");
    reset_window();
    {
        uint16_t p0a, p0b, p1;
        put_cmd(0, 0, XMSG_XFOPN);
        nd500_xmsg_service_mem(&g_mem); get_resp(r);
        p0a = nd500_xring_be16(r + RESP_ARG_A);
        put_cmd(1u << 1, 1, XMSG_XFOPN);
        nd500_xmsg_service_mem(&g_mem); get_resp(r);
        p1 = nd500_xring_be16(r + RESP_ARG_A);
        put_cmd(2u << 1, 0, XMSG_XFOPN);
        nd500_xmsg_service_mem(&g_mem); get_resp(r);
        p0b = nd500_xring_be16(r + RESP_ARG_A);
        check("sub-device 0 and 1 get different ports", p0a != p1);
        check("re-opening sub-device 0 returns the same port", p0a == p0b);
        check("no port is 0", p0a != 0 && p1 != 0);
    }

    /* A sub-device number outside xgtab[MAXXMSG] cannot legitimately reach us
     * (xgopen indexes xgtab with it), so it is refused rather than allowed to
     * index our own table. */
    reset_window();
    put_cmd(0, 99, XMSG_XFOPN);
    check_eq("answered", 1, nd500_xmsg_service_mem(&g_mem));
    check(" response present", get_resp(r) == 1);
    check("out-of-range sub-device is refused",
          (int16_t)nd500_xring_be16(r + RESP_ARG_T) < 0);

    /* ---- 6. An empty ring is not an error -------------------------------- */
    printf("\nan empty command ring\n");
    reset_window();
    check_eq("nothing to answer", 0, nd500_xmsg_service_mem(&g_mem));
    check("response ring untouched", nd500_xring_empty(&g_mem, &g_resp) == 1);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
