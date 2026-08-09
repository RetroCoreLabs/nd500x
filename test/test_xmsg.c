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

/* PHYSICAL memory, a separate space from the ring window. Buffers named inside
 * an xmsg_args are ND-100 WORD addresses converted with
 * `phys = word * 2 - FE_PRIVATE` (FE_PRIVATE = 0x2000), which does not go
 * through the ND-500 MMU - so it needs its own array here too. */
#define PHYS_LEN 0x4000u
static uint8_t g_phys[PHYS_LEN];

static uint8_t phys_read8(void* ctx, uint32_t a) {
    (void)ctx;
    return a < PHYS_LEN ? g_phys[a] : 0;
}
static void phys_write8(void* ctx, uint32_t a, uint8_t v) {
    (void)ctx;
    if (a < PHYS_LEN) g_phys[a] = v;
}

/* The word address whose physical translation is `phys`. */
#define PHYS_TO_WORD(phys) ((uint16_t)(((phys) + 0x2000u) / 2u))

static Nd500XmsgOps g_ops;

/* Command/response field offsets, repeated here on purpose: the test should
 * fail if nd500_xmsg.c's private copies ever drift from if/xmsg.h. */
#define CMD_SEQWORD 0
#define CMD_SUBDEV  4
#define CMD_FUNC    6
#define CMD_ARG_T   8
#define CMD_ARG_A  10
#define CMD_ARG_D  12
#define CMD_ARG_X  14
#define CMD_MAGNO  16
#define RESP_SEQWORD 0
#define RESP_SUBDEV  4
#define RESP_FUNC    6
#define RESP_ARG_T   8
#define RESP_ARG_A  10
#define RESP_CBA    16

static Nd500XRing g_cmd, g_resp;

/* Put one command in the ring exactly as NDIX's R_put would (if/xg.c:471-480):
 * seq word, sub-device, func, then the four argument registers, then magno.
 * Argument order here is the STRUCT order T, A, D, X - not xma()'s (T, A, X, D)
 * calling order, which is the trap this file exists partly to keep straight. */
static void put_cmd_args(uint32_t seqword, uint16_t subdev, uint16_t func,
                         uint16_t A, uint16_t D, uint16_t X) {
    uint8_t e[XRING_CMD_SIZE];
    memset(e, 0, sizeof e);
    nd500_xring_put_be32(e + CMD_SEQWORD, seqword);
    nd500_xring_put_be16(e + CMD_SUBDEV, subdev);
    nd500_xring_put_be16(e + CMD_FUNC, func);
    nd500_xring_put_be16(e + CMD_ARG_T, func);   /* xp->args = *ap, so T == func */
    nd500_xring_put_be16(e + CMD_ARG_A, A);
    nd500_xring_put_be16(e + CMD_ARG_D, D);
    nd500_xring_put_be16(e + CMD_ARG_X, X);
    nd500_xring_put_be32(e + CMD_MAGNO, 0);
    check("command accepted into the ring", nd500_xring_put(&g_mem, &g_cmd, e) == 1);
}
static void put_cmd(uint32_t seqword, uint16_t subdev, uint16_t func) {
    put_cmd_args(seqword, subdev, func, 0, 0, 0);
}

/* Take the next response out, the way xgintr()/R_get would. */
static int get_resp(uint8_t* out) {
    return nd500_xring_get(&g_mem, &g_resp, out);
}

static void reset_window(void) {
    memset(g_win, 0, sizeof g_win);
    memset(g_phys, 0, sizeof g_phys);
    nd500_xring_cmd_init(&g_cmd);
    nd500_xring_resp_init(&g_resp);
    nd500_xring_write_headers(&g_mem, &g_cmd);
    nd500_xring_write_headers(&g_mem, &g_resp);
    g_ops.ring    = g_mem;
    g_ops.pread8  = phys_read8;
    g_ops.pwrite8 = phys_write8;
    g_ops.ctx     = NULL;
    nd500_xmsg_reset();
}

/* Open a port on a sub-device and throw the response away - the precondition
 * for every message-space command. */
static void open_port(uint16_t subdev) {
    uint8_t r[XRING_RESP_SIZE];
    put_cmd(0, subdev, XMSG_XFOPN);
    nd500_xmsg_service_mem(&g_ops);
    get_resp(r);
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
    check_eq("one command answered", 1, nd500_xmsg_service_mem(&g_ops));
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
    check_eq("answered", 1, nd500_xmsg_service_mem(&g_ops));
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
    check_eq("answered", 1, nd500_xmsg_service_mem(&g_ops));
    check(" response present", get_resp(r) == 1);
    check_eq("func echoed WITH its option bits", (long)(060 | 0x8000 | 0x1000),
             (long)nd500_xring_be16(r + RESP_FUNC));

    /* ---- 3. An unknown function is answered, not dropped ------------------ */
    printf("\nunknown functions still get an answer\n");
    reset_window();
    put_cmd(4u << 1, 0, 055 /* XETHER - not implemented yet */);
    check_eq("answered", 1, nd500_xmsg_service_mem(&g_ops));
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
        check_eq("all 40 answered in one call", 40, nd500_xmsg_service_mem(&g_ops));
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
        nd500_xmsg_service_mem(&g_ops); get_resp(r);
        p0a = nd500_xring_be16(r + RESP_ARG_A);
        put_cmd(1u << 1, 1, XMSG_XFOPN);
        nd500_xmsg_service_mem(&g_ops); get_resp(r);
        p1 = nd500_xring_be16(r + RESP_ARG_A);
        put_cmd(2u << 1, 0, XMSG_XFOPN);
        nd500_xmsg_service_mem(&g_ops); get_resp(r);
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
    check_eq("answered", 1, nd500_xmsg_service_mem(&g_ops));
    check(" response present", get_resp(r) == 1);
    check("out-of-range sub-device is refused",
          (int16_t)nd500_xring_be16(r + RESP_ARG_T) < 0);

    /* ---- 6. Message space: XFGET / XFWRI / XFREL --------------------------
     * The attach sequence (if_et.c:347-395) is XFGET(1520) -> XFWRI(letter) ->
     * XFSND. Measured arriving from a real guest as:
     *     func=02 A=0x05F0 D=0x0000        (0x5F0 = 1520 = sizeof ei_dgram)
     *     func=07 A=0x1D1E D=0x001E        (0x1E  = 30 = the attach letter)
     * so A is the size for XFGET, and the buffer/length pair for XFWRI. */
    printf("\nmessage space\n");
    reset_window();
    open_port(0);
    {
        /* Lay the attach letter down in physical memory exactly as if_et.c
         * builds it, and hand its WORD address to XFWRI. */
        static const uint8_t letter[30] = {
            0x00, 0x65, 0x00, 0x08,               /* xr_header{0, XSLET, 8}    */
            0xFF, 0x06,                           /* xr_param{-1 string, 6}    */
            '*', 'E', 'N', 'U', 'M', '0',         /* the destination name      */
            0x00, 0x81,                           /* EXMHDtype = EXMTYattach   */
            0x00, 0x00, 0x00, 0x00,               /* identifier, length        */
            0x02, 0x60, 0x8C, 0x11, 0x22, 0x33,   /* our ethernet address      */
            0x00, 0x00, 0x00, 0x01, 0x00, 0x00    /* logical, valid=1, dum2    */
        };
        const uint32_t at = 0x1000;               /* somewhere in the array    */
        int i;
        for (i = 0; i < 30; i++) g_phys[at + i] = letter[i];

        put_cmd_args(1u << 1, 0, XMSG_XFGET, 1520, 0, 0);
        check_eq("XFGET answered", 1, nd500_xmsg_service_mem(&g_ops));
        check(" response present", get_resp(r) == 1);
        check_eq("XFGET succeeds", 0, (int16_t)nd500_xring_be16(r + RESP_ARG_T));

        put_cmd_args(2u << 1, 0, XMSG_XFWRI, PHYS_TO_WORD(at), 30, 0);
        check_eq("XFWRI answered", 1, nd500_xmsg_service_mem(&g_ops));
        check(" response present", get_resp(r) == 1);
        check_eq("XFWRI succeeds", 0, (int16_t)nd500_xring_be16(r + RESP_ARG_T));

        /* A second XFGET without an XFREL would be the driver leaking a
         * message, and silently accepting it would throw away whatever the
         * first one held. */
        put_cmd_args(3u << 1, 0, XMSG_XFGET, 1520, 0, 0);
        nd500_xmsg_service_mem(&g_ops); get_resp(r);
        check("a second XFGET while one is open is refused",
              (int16_t)nd500_xring_be16(r + RESP_ARG_T) < 0);

        put_cmd_args(4u << 1, 0, XMSG_XFREL, 0xFFFF /* XMCXM */, 0, 0);
        nd500_xmsg_service_mem(&g_ops); get_resp(r);
        check_eq("XFREL succeeds", 0, (int16_t)nd500_xring_be16(r + RESP_ARG_T));

        put_cmd_args(5u << 1, 0, XMSG_XFGET, 1520, 0, 0);
        nd500_xmsg_service_mem(&g_ops); get_resp(r);
        check_eq("XFGET works again after XFREL", 0,
                 (int16_t)nd500_xring_be16(r + RESP_ARG_T));
    }

    /* XFWRI with no message open, and a length past the message, are both
     * errors rather than silent overruns. */
    reset_window();
    open_port(0);
    put_cmd_args(0, 0, XMSG_XFWRI, PHYS_TO_WORD(0x1000), 30, 0);
    nd500_xmsg_service_mem(&g_ops); get_resp(r);
    check("XFWRI with no message open is refused",
          (int16_t)nd500_xring_be16(r + RESP_ARG_T) < 0);

    reset_window();
    open_port(0);
    put_cmd_args(0, 0, XMSG_XFGET, 16, 0, 0);
    nd500_xmsg_service_mem(&g_ops); get_resp(r);
    put_cmd_args(1u << 1, 0, XMSG_XFWRI, PHYS_TO_WORD(0x1000), 64, 0);
    nd500_xmsg_service_mem(&g_ops); get_resp(r);
    check("XFWRI past the end of the message is refused",
          (int16_t)nd500_xring_be16(r + RESP_ARG_T) < 0);

    /* Message space belongs to a PORT. Without XFOPN there is nothing to get. */
    reset_window();
    put_cmd_args(0, 0, XMSG_XFGET, 1520, 0, 0);
    nd500_xmsg_service_mem(&g_ops); get_resp(r);
    check("XFGET before XFOPN is refused",
          (int16_t)nd500_xring_be16(r + RESP_ARG_T) < 0);

    /* ---- 7. Putting the high bits back on a truncated address -------------
     * struct xmsg_args is all `short` (if/xmsg.h:25-30) while xma() takes the
     * arguments as `long` and assigns straight in (if_et.c:1173), so a word
     * address wider than 16 bits is truncated before it ever arrives.
     * MEASURED from a running guest: the attach letter was at word 0x00151D1E
     * and XFWRI carried A = 0x1D1E. dton(&xdata[sub]) from FE_OPEN is the one
     * full-width address this side is ever given, so it supplies the rest. */
    printf("\ntruncated word addresses\n");
    reset_window();
    nd500_xmsg_note_datbuf(0, 0x00151000u);      /* the measured neighbourhood */
    check_eq("the real attach-letter address is recovered",
             (long)0x00151D1Eu, (long)nd500_xmsg_full_word(0, 0x1D1E));

    /* Nearest-candidate, not mask-in-the-high-bits. With a base just above a
     * 64K word boundary, an address just below it must resolve DOWN - masking
     * would put it 64K words (128 KB) away, silently, and read as rubbish. */
    reset_window();
    nd500_xmsg_note_datbuf(0, 0x00150010u);
    check_eq("an address just below the base's 64K boundary resolves down",
             (long)0x0014FFF0u, (long)nd500_xmsg_full_word(0, 0xFFF0));
    reset_window();
    nd500_xmsg_note_datbuf(0, 0x0014FFF0u);
    check_eq("and just above it resolves up",
             (long)0x00150010u, (long)nd500_xmsg_full_word(0, 0x0010));

    /* With no FE_OPEN seen there is nothing to complete against. Returning the
     * address unchanged is visibly wrong rather than subtly wrong. */
    reset_window();
    check_eq("no base recorded: unchanged",
             (long)0x1D1Eu, (long)nd500_xmsg_full_word(0, 0x1D1E));

    /* ---- 8. An empty ring is not an error -------------------------------- */
    printf("\nan empty command ring\n");
    reset_window();
    check_eq("nothing to answer", 0, nd500_xmsg_service_mem(&g_ops));
    check("response ring untouched", nd500_xring_empty(&g_mem, &g_resp) == 1);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
