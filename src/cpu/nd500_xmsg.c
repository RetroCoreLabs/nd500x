/*
 * nd500_xmsg.c - the ND-100 side of NDIX's XMSG interface. See nd500_xmsg.h
 * for the contract, and nd500_xring.h for the ring discipline this sits on.
 *
 * The whole file is "what would the ND-100's XMSG kernel have said?". There is
 * no ND-100 here, so we say it.
 */

#include <string.h>
#include <stdio.h>

#include "nd500_xmsg.h"
#include "nd500_xring.h"
#include "cpu_protos.h"
#include "instruction_helpers.h"   /* nd500_read_memory_8 / nd500_write_memory_8 */
#include "../machine/machine_protos.h"  /* nd500_bus_read8 / nd500_bus_write8    */
#include "nd500_settings.h"        /* ND500X_FEDBG                               */

/* Byte offsets inside a command entry (struct xmsg_cmd, if/xmsg.h:35-42).
 * `long seq:31, fl:1` is one 32-bit word; PCC-500 packs bitfields from the top,
 * so seq is bits 31..1 and fl is bit 0. We never take those apart - the word is
 * echoed back verbatim, which is both correct and impossible to get wrong. */
#define CMD_SEQWORD   0
#define CMD_SUBDEV    4
#define CMD_FUNC      6
#define CMD_ARG_T     8
#define CMD_ARG_A    10
#define CMD_ARG_D    12
#define CMD_ARG_X    14
#define CMD_MAGNO    16

/* Byte offsets inside a response entry (struct xmsg_resp, if/xmsg.h:47-54).
 * Same head as a command; `cba` replaces `magno` and is a short, which is why
 * a response is 18 bytes and a command 20. */
#define RESP_SEQWORD  0
#define RESP_SUBDEV   4
#define RESP_FUNC     6
#define RESP_ARG_T    8
#define RESP_ARG_A   10
#define RESP_ARG_D   12
#define RESP_ARG_X   14
#define RESP_CBA     16

/* MAXXMSG (machine/fevar.h:96) - how many XMSG sub-devices NDIX configures.
 * xgtab[] is this long and xgopen() indexes it with the sub-device number, so a
 * sub-device outside this range cannot legitimately reach us. */
#define XMSG_MAX_SUBDEV 8

/* XMBSIZE (if/xbuf.h:13-22) - the biggest message NDIX ever asks for. The one
 * XFGET the ethernet driver issues is sizeof(struct ei_dgram) = 1520 (measured:
 * "func=02 A=0x05F0"), so this has room to spare. */
#define XMSG_MSG_MAX 2600

/* ---- server state --------------------------------------------------------
 * Deliberately file-static, and single-machine. nd500x runs one Nd500Machine
 * per process everywhere it is used today (the native frontend, the debugger
 * and the wasm build all create exactly one), and nd500_fecall.c already keeps
 * per-run state the same way. nd500_xmsg_reset() is called from the boot path
 * so a second boot in one process starts clean rather than inheriting state.
 *
 * "Current message" is an XMSG concept, not an invention here: XMCXM is -1,
 * "Current XMSG Message" (if/xmsg.h:167), and if_et.c never names a message -
 * XFGET creates one, XFWRI writes into it, XFSND sends it, XFREL frees it. So
 * one message per sub-device is exactly what the driver uses. */
/* An ac_ares / ac_dres / ac_mres is 18 bytes (if/if_access.h:42-50) and every
 * reply this server produces today is one of those three. Sized generously so
 * a data reply can share the slot later without a second mechanism. */
#define XMSG_REPLY_MAX 64

typedef struct XmsgSub {
    uint32_t datbuf_word;  /* full-width dton(&xdata[sub]) from FE_OPEN, 0=unset */
    uint16_t port;         /* 0 = no port open (XFOPN not yet done)          */
    int      msg_open;     /* XFGET done and not yet XFREL                   */
    uint16_t msg_size;     /* the size XFGET asked for                       */
    uint16_t msg_len;      /* bytes written into it so far by XFWRI          */
    uint8_t  msg[XMSG_MSG_MAX];

    /* What the next receive will be given. XFSND queues it; the XFRRE that
     * follows hands it over. The driver's own order guarantees the queue is
     * never deeper than one: xmsg() SLEEPS on each command, so the XFSND is
     * fully answered before the XFRRE is even built (if_et.c:393-400). */
    uint8_t  reply[XMSG_REPLY_MAX];
    uint16_t reply_len;    /* 0 = nothing waiting to be received             */

    /* Attach state, kept so a later XETHER can be told which address it is
     * sending from and a detach can be recognised as a state change rather
     * than just another letter. */
    int      attached;
    uint8_t  mac[6];

    /* A receive command that arrived with nothing to give it. It is PARKED,
     * not answered - see xmsg_park_receive() for why that is the correct
     * behaviour and not a wedge. */
    int      recv_parked;
    uint8_t  recv_cmd[XRING_CMD_SIZE];
} XmsgSub;

static XmsgSub g_sub[XMSG_MAX_SUBDEV];
static uint16_t g_next_port = 1;                    /* port 0 is not handed out */
static uint16_t g_next_msgid = 1;    /* what a receive reports in D (ei_xmid) */

/* ---- frames waiting to be received ---------------------------------------
 * NDIX keeps a receive outstanding at all times, but not at EVERY instant:
 * etrint() completes one, copies the datagram out, releases the message and
 * only then posts the next XFRREN (if_et.c:756-768). A frame arriving inside
 * that window has nowhere to go, so there is a short queue for it.
 *
 * Eight is chosen to cover that window and a burst behind it, not to be a
 * buffer. If frames are being dropped the guest is not keeping up, and the
 * counter says so rather than the packets just going missing. */
#define XMSG_RXQ_DEPTH  8
#define XMSG_FRAME_MAX  1536         /* ETHERMTU 1500 + a 14-byte header, rounded */

typedef struct XmsgFrame {
    uint8_t  buf[XMSG_FRAME_MAX];
    uint16_t len;
} XmsgFrame;

static XmsgFrame     g_rxq[XMSG_RXQ_DEPTH];
static int           g_rxq_head;
static int           g_rxq_count;
static unsigned long g_rx_dropped;

void nd500_xmsg_reset(void) {
    memset(g_sub, 0, sizeof g_sub);
    g_next_port = 1;
    g_next_msgid = 1;
    g_rxq_head = 0;
    g_rxq_count = 0;
    g_rx_dropped = 0;
}

static int xmsgdbg(void) {
    /* Shares ND500X_FEDBG rather than inventing another knob: every XMSG
     * command arrives as a fecall, so anyone tracing one wants the other. */
    static int v = -1;
    if (v < 0) v = nd500_settings()->fedbg;
    return v;
}

/* ---- guest memory, as the ring code wants it -----------------------------
 * The rings are at FIXED ND-500 VIRTUAL addresses in segment 6 (0x30000000 and
 * 0x30000800). Their physical addresses are demand-allocated and move, so this
 * goes through the MMU every time. Reading them at a remembered physical
 * address returns plausible rubbish rather than an error - that mistake has
 * already been made once here, see REFERENCE_XMSG_ADDRESSES_2026-08-09.md. */
static uint8_t xm_read8(void* ctx, uint32_t vaddr) {
    return nd500_read_memory_8((Nd500Cpu*)ctx, vaddr);
}
static void xm_write8(void* ctx, uint32_t vaddr, uint8_t val) {
    nd500_write_memory_8((Nd500Cpu*)ctx, vaddr, val);
}

/* PHYSICAL memory, for the buffers named inside an xmsg_args. Those are ND-100
 * word addresses and bypass the ND-500 MMU entirely - see Nd500XmsgOps. */
static uint8_t xm_pread8(void* ctx, uint32_t phys) {
    return nd500_bus_read8(((Nd500Cpu*)ctx)->machine, phys);
}
static void xm_pwrite8(void* ctx, uint32_t phys, uint8_t val) {
    nd500_bus_write8(((Nd500Cpu*)ctx)->machine, phys, val);
}

/* ND-100 word address -> ND-500 physical byte address. FE_PRIVATE (0x2000) is
 * the offset the two sides agreed on at FE_INIT; nd500_fecall.c does exactly
 * this at :545 for the tty buffers and at :1793 for disk. */
#define XMSG_FE_PRIVATE 0x00002000u
static uint32_t xmsg_word_to_phys(uint32_t word_addr) {
    return word_addr * 2u - XMSG_FE_PRIVATE;
}

void nd500_xmsg_note_datbuf(uint32_t subdev, uint32_t datbuf_word) {
    if (subdev < XMSG_MAX_SUBDEV) g_sub[subdev].datbuf_word = datbuf_word;
}

/*
 * Put the high bits back on a truncated word address.
 *
 * MEASURED, not assumed (2026-08-09, guest booted to a login prompt with an
 * `ifconfig et0 inet ... up`): the attach letter really sat at physical
 * 0x002A1A3C, i.e. word 0x00151D1E, and XFWRI carried `A = 0x1D1E`. The low 16
 * bits match exactly; 0x0015 was lost in the `short`.
 *
 * The base to complete against is dton(&xdata[sub]) from FE_OPEN. Rather than
 * OR the base's high bits in, this picks whichever of base_high-1, base_high or
 * base_high+1 lands NEAREST the base: a buffer a little BELOW a 64K word
 * boundary that the base sits just above would otherwise be reconstructed
 * 128 KB away, silently, and read as rubbish.
 *
 * With no base recorded the address is returned as-is. That is what the old
 * behaviour was, it is visibly wrong rather than subtly wrong, and there is
 * nothing better to do.
 */
uint32_t nd500_xmsg_full_word(uint32_t subdev, uint16_t truncated) {
    uint32_t base, best;
    long best_d;
    int i;

    if (subdev >= XMSG_MAX_SUBDEV || g_sub[subdev].datbuf_word == 0)
        return truncated;

    base = g_sub[subdev].datbuf_word;
    best = truncated;
    best_d = 0;
    for (i = -1; i <= 1; i++) {
        long high = (long)(base >> 16) + i;
        uint32_t cand;
        long d;
        if (high < 0) continue;
        cand = ((uint32_t)high << 16) | truncated;
        d = (long)cand - (long)base;
        if (d < 0) d = -d;
        if (i == -1 || d < best_d) { best = cand; best_d = d; }
    }
    return best;
}

/* ---- the one command we answer properly ---------------------------------- */

/*
 * XFOPN - open a port on this sub-device.
 *
 * if/if_et.c:175 issues it during etattach() and, on success, keeps the port
 * number: `es->es_portno = xa->A` (if_et.c:181). That port number is what every
 * later call uses - XFRREN passes it in A (if_et.c:486), XFSND in D. It is an
 * opaque token as far as NDIX is concerned, so any distinct non-zero value
 * works, and we hand out 1, 2, 3, ... in the order ports are opened.
 *
 * "at most 1 Xmsg port open on a channel" (if_et.c:141), so re-opening a
 * sub-device that already has a port simply returns the same one rather than
 * leaking a new number.
 */
static void xmsg_do_open(uint16_t subdev, uint16_t* out_T, uint16_t* out_A) {
    if (subdev >= XMSG_MAX_SUBDEV) {
        *out_T = (uint16_t)XMSG_XENOP;   /* no more ports available */
        *out_A = 0;
        return;
    }
    if (g_sub[subdev].port == 0)
        g_sub[subdev].port = g_next_port++;
    *out_T = (uint16_t)XMSG_XMSUX;       /* 0 - if_et.c tests `xa->T < 0` */
    *out_A = g_sub[subdev].port;
}

/*
 * XFGET - get message space, size in A.
 *
 * if_et.c:347 asks for sizeof(struct ei_dgram) = 1520 and nothing else, and it
 * never names the message afterwards: XFWRI writes into it, XFSND sends it,
 * XFREL frees it, all implicitly. That is XMSG's "current message" (XMCXM = -1,
 * "Current XMSG Message", if/xmsg.h:167), so one message per sub-device is all
 * the driver can use.
 *
 * A second XFGET while a message is already open would be the driver leaking
 * one. That has not been seen, so it is refused rather than silently accepted -
 * a silent accept would lose whatever the first message still held.
 */
static void xmsg_do_get(uint16_t subdev, uint16_t size,
                        uint16_t* out_T, uint16_t* out_A) {
    XmsgSub* s;
    *out_A = 0;
    if (subdev >= XMSG_MAX_SUBDEV || g_sub[subdev].port == 0) {
        *out_T = (uint16_t)XMSG_XENDP;   /* no port open on this sub-device */
        return;
    }
    s = &g_sub[subdev];
    if (size == 0 || size > XMSG_MSG_MAX) {
        *out_T = (uint16_t)XMSG_XEILM;   /* illegal message size */
        return;
    }
    if (s->msg_open) {
        *out_T = (uint16_t)XMSG_XEXBF;   /* already has a message buffer */
        return;
    }
    s->msg_open = 1;
    s->msg_size = size;
    s->msg_len  = 0;
    memset(s->msg, 0, sizeof s->msg);
    *out_T = (uint16_t)XMSG_XMSUX;
}

/*
 * XFWRI - copy bytes from a guest buffer into the current message.
 *
 * if_et.c:390 `xma(xa, XFWRI, dton(letter), 0, len)`. xma() is declared
 * (T, A, X, D) while struct xmsg_args is laid out T, A, D, X, so the third
 * argument lands in X and the fourth in D: **A = the buffer, D = the length**,
 * X = 0. Measured arriving as A=0x1D1E D=0x001E - and 0x1E = 30 is exactly the
 * attach letter: xr_header(4) + xr_param(2) + "*ENUM0"(6) + ac_areq(18).
 *
 * A is an ND-100 WORD address, and it is a `short` in the struct, so anything
 * above 0xFFFF words has already been truncated by the time it reaches us.
 * Whether that ever happens is checked by the dump below rather than assumed.
 */
static void xmsg_do_wri(const Nd500XmsgOps* ops, uint16_t subdev,
                        uint16_t waddr, uint16_t len, uint16_t* out_T) {
    XmsgSub* s;
    uint32_t phys;
    uint16_t i;

    if (subdev >= XMSG_MAX_SUBDEV || g_sub[subdev].port == 0) {
        *out_T = (uint16_t)XMSG_XENDP;
        return;
    }
    s = &g_sub[subdev];
    if (!s->msg_open) {
        *out_T = (uint16_t)XMSG_XENDM;   /* no default (current) message */
        return;
    }
    if ((uint32_t)s->msg_len + len > s->msg_size ||
        (uint32_t)s->msg_len + len > XMSG_MSG_MAX) {
        *out_T = (uint16_t)XMSG_XEITL;   /* illegal transfer length */
        return;
    }

    phys = xmsg_word_to_phys(nd500_xmsg_full_word(subdev, waddr));
    for (i = 0; i < len; i++)
        s->msg[s->msg_len + i] = ops->pread8(ops->ctx, phys + i);
    s->msg_len = (uint16_t)(s->msg_len + len);
    *out_T = (uint16_t)XMSG_XMSUX;

    if (xmsgdbg()) {
        /* The first XFWRI of the attach sequence is an XROUT letter whose shape
         * is known exactly (if_et.c:369-379, spec section 4.3):
         *   00 65 00 08   xr_header{serial=0, service=XSLET=0101, length=8}
         *   FF 06         xr_param {type=-1 (string), length=6}
         *   2A 45 4E 55 4D 30   "*ENUM0"
         *   ... 18 bytes of ac_areq, starting 00 81 (EXMTYattach = 129)
         * If the bytes below are not that, the word->physical arithmetic is
         * wrong and nothing downstream can be trusted - which is exactly the
         * kind of silent wrongness this whole area keeps producing. */
        uint16_t n = len > 32 ? 32 : len;
        fprintf(stderr, "[XMSG]   XFWRI %u bytes from word 0x%04X (phys 0x%08X):",
                len, waddr, phys);
        for (i = 0; i < n; i++)
            fprintf(stderr, " %02X", s->msg[s->msg_len - len + i]);
        fprintf(stderr, "%s\n", len > n ? " ..." : "");
    }
}

/*
 * XFREL - release message space. if_et.c passes A = -1 (XMCXM, "the current
 * message"), which is the only message there is. Releasing when nothing is open
 * is not an error worth failing on - the driver's error paths call it to clean
 * up after a failure, and refusing there would turn one problem into two.
 */
static void xmsg_do_rel(uint16_t subdev, uint16_t* out_T) {
    if (subdev >= XMSG_MAX_SUBDEV) { *out_T = (uint16_t)XMSG_XENDP; return; }
    g_sub[subdev].msg_open = 0;
    g_sub[subdev].msg_len  = 0;
    g_sub[subdev].msg_size = 0;
    *out_T = (uint16_t)XMSG_XMSUX;
}

/* ---- the attach handshake ------------------------------------------------- */

/*
 * Queue an 18-byte status reply (struct ac_ares / ac_dres / ac_mres - they are
 * the same shape, if/if_access.h:42-112) for the receive that is coming.
 *
 *   short EXMHDtype        EXMTYstatus (130)
 *   short EXMHDidentifier  echoed from the request, "for later ref"
 *   short EXMHDlength      length of data - 0
 *   char  EXMHDdummy[6]    not used
 *   short EXMSTdum1        not used
 *   short EXMSTstatus      the answer: EXMATok = 0
 *   short EXMSTdum2        not used
 *
 * if_et.c only ever looks at EXMHDtype (:404, :458, :673) and never at the
 * status word, but the status word is what the message MEANS, so it is filled
 * in properly rather than left zero by luck.
 */
static void xmsg_queue_status(XmsgSub* s, uint16_t identifier, uint16_t status) {
    memset(s->reply, 0, sizeof s->reply);
    nd500_xring_put_be16(s->reply + 0,  XMSG_EXMTYstatus);
    nd500_xring_put_be16(s->reply + 2,  identifier);
    nd500_xring_put_be16(s->reply + 4,  0);
    /* bytes 6..11 are EXMHDdummy, left zero */
    nd500_xring_put_be16(s->reply + 12, 0);       /* EXMSTdum1  */
    nd500_xring_put_be16(s->reply + 14, status);  /* EXMSTstatus */
    nd500_xring_put_be16(s->reply + 16, 0);       /* EXMSTdum2  */
    s->reply_len = 18;
}

/*
 * XFSND - send the current message.
 *
 *   attach/detach: xma(xa, XFSND|XFROU, 0, es->es_portno, 0)   (if_et.c:393)
 *                  xma() is (T, A, X, D) and the struct is T, A, D, X, so the
 *                  PORT arrives in X and A is 0. Measured: func=02014, X=1.
 *   multicast:     xma(xa, XFSND, es_magno>>16, es_portno, es_magno&0xffff)
 *                  (if_et.c:444) - no XFROU, straight to the interface, and
 *                  this one puts the port in X as well.
 *
 * With XFROU the message is a letter for XROUT and has to be unwrapped:
 *
 *   xr_header  4  serial, service, length   (char, char, short - if_param.h:17)
 *   xr_param   2  type (-1 = string), length
 *   name       6  "*ENUM0"
 *   ac_areq   18  the request proper
 *
 * Without XFROU there is no letter - the ac_mreq sits at offset 0 (if_et.c:437
 * bcopy's `bcast` straight into ex_buf).
 *
 * Whatever it turns out to be, a status reply is queued for the XFRRE that the
 * driver issues next. Queueing nothing would leave that XFRRE parked for ever
 * and etinit() asleep in sleep(es, PZERO+1) with nothing printed.
 */
static void xmsg_do_snd(uint16_t subdev, uint16_t func, uint16_t port,
                        uint16_t* out_T) {
    XmsgSub* s;
    const uint8_t* body;
    uint16_t body_len;
    uint16_t type, ident;

    if (subdev >= XMSG_MAX_SUBDEV || g_sub[subdev].port == 0) {
        *out_T = (uint16_t)XMSG_XENDP;
        return;
    }
    s = &g_sub[subdev];
    if (!s->msg_open || s->msg_len == 0) {
        *out_T = (uint16_t)XMSG_XENDM;   /* nothing to send */
        return;
    }
    if (port != 0 && port != s->port) {
        /* The driver always passes its own port back, so this cannot happen
         * while things are sane - and if it does, guessing would be worse. */
        *out_T = (uint16_t)XMSG_XENDP;
        return;
    }

    body = s->msg;
    body_len = s->msg_len;

    if (func & XMSG_XFROU) {
        /* Unwrap the letter. Everything is checked rather than assumed: a
         * mis-parse here produces a reply that looks right and means nothing,
         * which is the failure mode this whole area specialises in. */
        uint8_t service, ptype, plen;
        if (body_len < 12) { *out_T = (uint16_t)XMSG_XEILM; return; }
        service = body[1];
        ptype   = body[4];
        plen    = body[5];
        if (service != XMSG_XSLET) {
            fprintf(stderr, "[XMSG] XFROU letter service is 0%o, expected "
                            "XSLET 0%o - not answering it\n",
                    service, XMSG_XSLET);
            *out_T = (uint16_t)XMSG_XENIM;
            return;
        }
        if (ptype != 0xFF || plen == 0 || (uint16_t)(6 + plen) > body_len) {
            fprintf(stderr, "[XMSG] XFROU param block is type %d length %u - "
                            "expected a string of 6 (\"*ENUM0\")\n",
                    (int8_t)ptype, plen);
            *out_T = (uint16_t)XMSG_XEILM;
            return;
        }
        /* The name is "*ENUMi" where i is the unit's thumbwheel digit
         * (if_et.c:374, ET_NAM in if_etregs.h:57). We serve every unit, so the
         * digit is not compared - only the "*ENUM" that says this is meant for
         * the ethernet media server at all. */
        if (plen < 5 || memcmp(body + 6, "*ENUM", 5) != 0) {
            fprintf(stderr, "[XMSG] XFROU letter is addressed to \"%.*s\", "
                            "not *ENUMi - no server here\n", plen, body + 6);
            *out_T = (uint16_t)XMSG_XENIM;
            return;
        }
        body     = s->msg + 6 + plen;
        body_len = (uint16_t)(s->msg_len - (6 + plen));
    }

    if (body_len < 18) { *out_T = (uint16_t)XMSG_XEILM; return; }
    type  = nd500_xring_be16(body + 0);
    ident = nd500_xring_be16(body + 2);

    switch (type) {
    case XMSG_EXMTYattach:
        /* EXMHDaddress[6] at offset 6 is the address etconfig(8) set. Keep it:
         * a transmitted frame's source address is ours to fill in later. */
        memcpy(s->mac, body + 6, 6);
        s->attached = 1;
        if (xmsgdbg())
            fprintf(stderr, "[XMSG]   ATTACH %02X:%02X:%02X:%02X:%02X:%02X "
                            "on port %u -> EXMATok\n",
                    s->mac[0], s->mac[1], s->mac[2],
                    s->mac[3], s->mac[4], s->mac[5], s->port);
        xmsg_queue_status(s, ident, XMSG_EXMATok);
        break;
    case XMSG_EXMTYdetach:
        s->attached = 0;
        if (xmsgdbg())
            fprintf(stderr, "[XMSG]   DETACH on port %u -> EXMATok\n", s->port);
        xmsg_queue_status(s, ident, XMSG_EXMATok);
        break;
    case XMSG_EXMTYdefineMulti:
        /* if_et.c:437 defines ff:ff:ff:ff:ff:ff so it can hear broadcasts.
         * Nothing filters yet, so accepting is honest: everything IS delivered. */
        if (xmsgdbg())
            fprintf(stderr, "[XMSG]   DEFINE MULTICAST "
                            "%02X:%02X:%02X:%02X:%02X:%02X -> EXMATok\n",
                    body[6], body[7], body[8], body[9], body[10], body[11]);
        xmsg_queue_status(s, ident, XMSG_EXMATok);
        break;
    default:
        fprintf(stderr, "[XMSG] XFSND carries EXMHDtype %u, which is not "
                        "attach/detach/multicast - answering XENIM\n", type);
        *out_T = (uint16_t)XMSG_XENIM;
        return;
    }

    /* The message has been consumed. The BUFFER stays allocated: if_et.c:437
     * writes the multicast request into it with no XFGET in between ("we can
     * use the Xmsg buffer XFRRE'd from Attach to Server", :422). */
    s->msg_len = 0;
    *out_T = (uint16_t)XMSG_XMSUX;
}

/*
 * XFRRE / XFRREN - receive a message into a guest buffer.
 *
 *   xma(xr, XFRRE|XFWTF, es->es_portno, dton(ei->ei_recv), sizeof(ei_dgram))
 *   (if_et.c:398) -> A = port, X = buffer word address, D = buffer size.
 *
 * On the response etrint() reads (if_et.c:660-670):
 *   T < 0  an error
 *   D      the message id, kept as ei_xmid
 *   X      the RECEIVED LENGTH
 * and then looks at er_head.EXMHDtype in the buffer itself to tell an
 * attach/detach status reply from a datagram.
 *
 * With nothing to give it the command is PARKED, not answered - see
 * xmsg_park_receive().
 *
 * Returns 1 if a response should be built, 0 if the command was parked.
 */
/*
 * What is there to receive, if anything? Copies it into `out` and returns its
 * length, or 0 for "nothing waiting".
 *
 * Two sources, in priority order:
 *
 *   1. A status reply queued by XFSND. This is the attach handshake, and it
 *      must not be overtaken by a frame that happens to arrive first - etinit()
 *      gives up on anything whose EXMHDtype is not EXMTYstatus (if_et.c:404).
 *   2. A frame from the uplink, wrapped in the 6-byte ac_head envelope that
 *      XETHER strips on the way out.
 *
 * The envelope's EXMHDlength follows the SAME rule as transmit: etrint() reads
 * `len = er_head.EXMHDlength - sizeof(short)` and treats that as the payload
 * after the ethernet header (if_et.c:679), so EXMHDlength counts the 2-byte
 * ether_type - i.e. framelen - 12.
 */
static uint16_t xmsg_next_message(XmsgSub* s, uint8_t* out, uint16_t max) {
    XmsgFrame* f;
    uint16_t total;

    if (s->reply_len != 0) {
        uint16_t n = s->reply_len;
        if (n > max) return 0;
        memcpy(out, s->reply, n);
        s->reply_len = 0;
        return n;
    }
    /* A frame the guest's buffer cannot hold is DROPPED here rather than left
     * at the head of the queue. Leaving it would park every later receive
     * behind something that can never be delivered - a queue that stops for
     * good, which is the shape of failure this whole area keeps producing. */
    while (g_rxq_count > 0) {
        f = &g_rxq[g_rxq_head];
        total = (uint16_t)(f->len + 6);
        g_rxq_head = (g_rxq_head + 1) % XMSG_RXQ_DEPTH;
        g_rxq_count--;

        if (total > max) {
            g_rx_dropped++;
            fprintf(stderr, "[XMSG] a %u-byte frame does not fit the %u-byte "
                            "receive buffer - dropped (%lu dropped so far)\n",
                    total, max, g_rx_dropped);
            continue;
        }

        nd500_xring_put_be16(out + 0, XMSG_EXMTYdata);
        nd500_xring_put_be16(out + 2, 0);                       /* identifier */
        nd500_xring_put_be16(out + 4, (uint16_t)(f->len - 12)); /* EXMHDlength */
        memcpy(out + 6, f->buf, f->len);
        return total;
    }
    return 0;
}

static int xmsg_do_recv(const Nd500XmsgOps* ops, uint16_t subdev,
                        uint16_t port, uint16_t waddr, uint16_t size,
                        const uint8_t* centry,
                        uint16_t* out_T, uint16_t* out_D, uint16_t* out_X) {
    XmsgSub* s;
    uint32_t phys;
    uint16_t i, len;
    static uint8_t msg[XMSG_FRAME_MAX + 16];

    if (subdev >= XMSG_MAX_SUBDEV || g_sub[subdev].port == 0) {
        *out_T = (uint16_t)XMSG_XENDP;
        return 1;
    }
    s = &g_sub[subdev];
    if (port != s->port) { *out_T = (uint16_t)XMSG_XENDP; return 1; }

    /* The guest's buffer size bounds what can be handed over, so it is what
     * limits the copy - not the size of our own scratch. */
    len = xmsg_next_message(s, msg,
                            size < sizeof msg ? size : (uint16_t)sizeof msg);
    if (len == 0) {
        /* Nothing has arrived for this port. A real XMSG front end simply does
         * not complete the request until something does, and that is what is
         * modelled here: the command is remembered and answered later.
         *
         * This is NOT the wedge described in the header. A wedge is an
         * outstanding request that can never be retired; this one is retired
         * the moment there is a message, and xgdctl's HAS_RCV flag being set
         * meanwhile is exactly right - a receive IS outstanding. */
        if (s->recv_parked)
            fprintf(stderr, "[XMSG] second receive parked on subdev %u - the "
                            "first one is being dropped\n", subdev);
        memcpy(s->recv_cmd, centry, XRING_CMD_SIZE);
        s->recv_parked = 1;
        if (xmsgdbg())
            /* The seq is printed here because a parked command produces no
             * "cmd seq=..." line of its own, and without it the trace looks
             * like a command went missing. */
            fprintf(stderr, "[XMSG]   receive PARKED: seq=%u port %u "
                            "(buffer word 0x%04X, %u bytes) - nothing to give "
                            "it yet\n",
                    nd500_xring_be32(centry + CMD_SEQWORD) >> 1,
                    port, waddr, size);
        return 0;
    }

    phys = xmsg_word_to_phys(nd500_xmsg_full_word(subdev, waddr));
    for (i = 0; i < len; i++)
        ops->pwrite8(ops->ctx, phys + i, msg[i]);

    *out_T = (uint16_t)XMSG_XMSUX;
    *out_X = len;               /* the received length, etrint reads it from X */
    *out_D = g_next_msgid++;    /* the message id, kept as ei_xmid            */
    if (g_next_msgid == 0) g_next_msgid = 1;

    if (xmsgdbg()) {
        uint16_t n = len > 32 ? 32 : len;
        fprintf(stderr, "[XMSG]   receive %u bytes to word 0x%04X "
                        "(phys 0x%08X):", len, waddr, phys);
        for (i = 0; i < n; i++)
            fprintf(stderr, " %02X", msg[i]);
        fprintf(stderr, "%s\n", len > n ? " ..." : "");
    }

    /* The received message becomes the current message - that is why if_et.c
     * can XFWRI the multicast request without an XFGET first (:422, :437). */
    s->msg_open = 1;
    if (s->msg_size == 0) s->msg_size = XMSG_MSG_MAX;
    s->msg_len  = 0;
    return 1;
}

/* ---- transmit -------------------------------------------------------------
 *
 * XETHER - "here is a framed datagram, put it on the wire".
 *
 *   xgdctl(unit, xma(xa, XETHER, dton(ei->ei_xmit), xlen, es->es_portno),
 *          es->es_magno, DONTWAIT);                          (if_et.c:539)
 *
 * xma() is (T, A, X, D), so: A = buffer, X = LENGTH, D = port. Measured on the
 * wire as A=0x1D14 D=0x0001 X=0x0040 - the first ARP, 64 bytes.
 *
 * ONE command. No XFGET, no XFWRI, no XFSND - the whole datagram travels by
 * address. It is a `struct ei_dgram` (if_etregs.h:33-37):
 *
 *   +0   struct ac_head       6   EXMTYdata, identifier, EXMHDlength
 *   +6   struct ether_header 14   destination, source, type
 *   +20  the payload
 *
 * and xlen counts from +0: `EXMHDlength + sizeof(ac_head) + sizeof(ether_header)
 * - 2` (if_et.c:530). The frame proper is therefore xlen - 6 bytes starting at
 * +6, and the ac_head is XMSG's envelope, not part of the ethernet frame.
 *
 * A NOTE ON THE MINIMUM LENGTH, because it looks like a bug and is not ours.
 * etput() sets EXMHDlength = off - 12 and clamps it to ETHERMIN = 60-14 = 46
 * (if_et.c:953-955, netinet/if_ether.h:35). Since EXMHDlength counts the
 * 2-byte ether_type, that clamp means payload >= 44 and a frame of 58 bytes -
 * TWO SHORT of the 60-byte ethernet minimum. NDIX has always done this. The
 * frame is passed on exactly as built; padding to 60 belongs in whatever uplink
 * needs it, not here, where it would quietly change what the guest sent.
 */
static void xmsg_do_ether(const Nd500XmsgOps* ops, uint16_t subdev,
                          uint16_t waddr, uint16_t port, uint16_t xlen,
                          uint16_t* out_T) {
    static uint8_t frame[XMSG_MSG_MAX];
    uint32_t phys;
    uint16_t i, type, hdrlen, framelen;

    if (subdev >= XMSG_MAX_SUBDEV || g_sub[subdev].port == 0) {
        *out_T = (uint16_t)XMSG_XENDP;
        return;
    }
    if (port != g_sub[subdev].port) { *out_T = (uint16_t)XMSG_XENDP; return; }

    /* Below the envelope plus an ethernet header there is no frame at all. */
    if (xlen < 6 + 14 || xlen > XMSG_MSG_MAX) {
        *out_T = (uint16_t)XMSG_XEITL;
        return;
    }

    phys = xmsg_word_to_phys(nd500_xmsg_full_word(subdev, waddr));
    for (i = 0; i < xlen; i++)
        frame[i] = ops->pread8(ops->ctx, phys + i);

    type   = nd500_xring_be16(frame + 0);
    hdrlen = nd500_xring_be16(frame + 4);
    if (type != XMSG_EXMTYdata) {
        /* Anything else means the address arithmetic put us somewhere that is
         * not a datagram. Sending 58 bytes of whatever-that-was would be worse
         * than refusing, and the refusal is printed by etxint(). */
        fprintf(stderr, "[XMSG] XETHER buffer at word 0x%04X (phys 0x%08X) has "
                        "EXMHDtype %u, not EXMTYdata - not transmitting it\n",
                waddr, phys, type);
        *out_T = (uint16_t)XMSG_XEILM;
        return;
    }
    if ((uint32_t)hdrlen + 18 != (uint32_t)xlen)
        /* Only a warning: xlen is what the driver asked us to send, and it is
         * the authority. This says the two disagree, which is worth knowing. */
        fprintf(stderr, "[XMSG] XETHER length disagreement: EXMHDlength=%u "
                        "implies %u bytes, command says %u\n",
                hdrlen, hdrlen + 18, xlen);

    framelen = (uint16_t)(xlen - 6);

    if (xmsgdbg()) {
        uint16_t n = framelen > 32 ? 32 : framelen;
        fprintf(stderr, "[XMSG]   XETHER %u bytes on the wire "
                        "(%02X:%02X:%02X:%02X:%02X:%02X <- "
                        "%02X:%02X:%02X:%02X:%02X:%02X type %04X):",
                framelen,
                frame[6], frame[7], frame[8], frame[9], frame[10], frame[11],
                frame[12], frame[13], frame[14], frame[15], frame[16], frame[17],
                nd500_xring_be16(frame + 18));
        for (i = 0; i < n; i++) fprintf(stderr, " %02X", frame[6 + i]);
        fprintf(stderr, "%s\n", framelen > n ? " ..." : "");
    }

    if (ops->frame_out)
        ops->frame_out(ops->frame_ctx, frame + 6, framelen);

    /* Success either way. With no uplink the frame is dropped, and saying so
     * with an error would make etxint() count an output error for something
     * NDIX did correctly. */
    *out_T = (uint16_t)XMSG_XMSUX;
}

/*
 * XFMST - message status. `xma(xa, XFMST, -1, 0, 0)` (if_et.c:412), and the
 * driver keeps `es_magno = xa->A << 16 | xa->D` (:417).
 *
 * THE TWO HALVES ARE NOT ARBITRARY. This used to answer A=1, D=port on the
 * grounds that NDIX never looks inside the magic number - which is true, it is
 * only echoed back in the `magno` field of later commands. But it is a real
 * address on a real machine, and RetroCore's oracle capture of the actual 68K
 * ENCOS firmware shows what it is made of:
 *
 *     XFMST -> T=0x0001 A=0x0064 D=0x02AF
 *     XFSND(A=0x0064, X=4)   "send to the kernel magic (node 100), from port 4"
 *
 * 0x0064 = 100 = the SYSTEM NUMBER of the node that capture was taken on.
 * (RetroCore, Emulated.HW/ND/CPU/NDBUS/EthernetII/ETHII-HLE-PROTOCOL-SPEC.md.)
 * So A is the ND system number and D identifies the port. Answering 1 was
 * harmless for NDIX alone and wrong the moment anything real is on the far end
 * - and wrong in a way nobody would notice until a peer rejected it.
 *
 * Both halves must stay clear of bit 15: NDIX combines them with a SIGNED shift
 * of a `short`, so a high bit would sign-extend and smear the two together.
 * A system number that large would break a real machine too.
 */
static void xmsg_do_mst(uint16_t subdev, uint16_t* out_T,
                        uint16_t* out_A, uint16_t* out_D) {
    uint32_t sysno;

    if (subdev >= XMSG_MAX_SUBDEV || g_sub[subdev].port == 0) {
        *out_T = (uint16_t)XMSG_XENDP;
        return;
    }
    sysno = nd500_settings()->sysno;
    if (sysno == 0 || sysno > 0x7FFF) sysno = 500;   /* see the bit-15 note */

    *out_T = (uint16_t)XMSG_XMSUX;
    *out_A = (uint16_t)sysno;
    *out_D = g_sub[subdev].port;
}

/* ---- the service loop ---------------------------------------------------- */

/* The uplink, if anything has plugged one in. NULL is "uplink_null": frames are
 * logged under ND500X_FEDBG and dropped, which is what phases 1-4 need and is
 * why nothing has to be configured to get this far. */
static Nd500XmsgFrameOut g_uplink_fn  = NULL;
static void*             g_uplink_ctx = NULL;

static Nd500XmsgUplinkPoll g_uplink_poll_fn  = NULL;
static void*               g_uplink_poll_ctx = NULL;

void nd500_xmsg_set_uplink(Nd500XmsgFrameOut fn, void* ctx) {
    g_uplink_fn  = fn;
    g_uplink_ctx = ctx;
}

void nd500_xmsg_set_uplink_poll(Nd500XmsgUplinkPoll fn, void* ctx) {
    g_uplink_poll_fn  = fn;
    g_uplink_poll_ctx = ctx;
}

void nd500_xmsg_uplink_poll(Nd500Cpu* cpu) {
    if (g_uplink_poll_fn) g_uplink_poll_fn(g_uplink_poll_ctx, cpu);
}

int nd500_xmsg_service(Nd500Cpu* cpu) {
    Nd500XmsgOps ops;
    if (!cpu) return 0;
    ops.ring.read8  = xm_read8;
    ops.ring.write8 = xm_write8;
    ops.ring.ctx    = cpu;
    ops.pread8      = xm_pread8;
    ops.pwrite8     = xm_pwrite8;
    ops.ctx         = cpu;
    ops.frame_out   = g_uplink_fn;
    ops.frame_ctx   = g_uplink_ctx;
    return nd500_xmsg_service_mem(&ops);
}

/*
 * Put one answer in the response ring. Returns 1 on success, 0 if the ring was
 * full.
 *
 * seq, subdev and func are echoed VERBATIM out of the command being answered.
 * xgintr() picks which outstanding-request flag to clear from the RESPONSE's
 * own func (xg.c:374), and a mismatch there wedges the sub-device silently.
 * Do not "improve" that.
 *
 * Split out of the service loop because a parked receive is completed from
 * somewhere else entirely - a frame arriving from the uplink, with no command
 * ring involved - and it must produce byte-for-byte the same answer.
 */
static int xmsg_put_response(const Nd500XmsgOps* ops, const uint8_t* centry,
                             uint16_t T, uint16_t A, uint16_t D, uint16_t X) {
    Nd500XRing resp;
    uint8_t rentry[XRING_RESP_SIZE];

    nd500_xring_resp_init(&resp);

    memset(rentry, 0, sizeof rentry);
    memcpy(rentry + RESP_SEQWORD, centry + CMD_SEQWORD, 4);
    memcpy(rentry + RESP_SUBDEV,  centry + CMD_SUBDEV, 2);
    memcpy(rentry + RESP_FUNC,    centry + CMD_FUNC, 2);
    nd500_xring_put_be16(rentry + RESP_ARG_T, T);
    nd500_xring_put_be16(rentry + RESP_ARG_A, A);
    nd500_xring_put_be16(rentry + RESP_ARG_D, D);
    nd500_xring_put_be16(rentry + RESP_ARG_X, X);
    nd500_xring_put_be16(rentry + RESP_CBA,   0);

    if (nd500_xring_put(&ops->ring, &resp, rentry))
        return 1;

    /* The response ring is full: 113 answers are outstanding and NDIX has not
     * run xgintr() once. That cannot happen while the guest is healthy, and
     * dropping the answer would wedge the sub-device, so say so loudly rather
     * than lose it quietly. */
    fprintf(stderr, "[XMSG] response ring FULL - answer to subdev=%u "
                    "func=0%o DROPPED (that sub-device is now stuck)\n",
            nd500_xring_be16(centry + CMD_SUBDEV),
            nd500_xring_be16(centry + CMD_FUNC));
    return 0;
}

/*
 * A frame has arrived from the uplink. Queue it, and complete any receive that
 * is parked waiting for one.
 *
 * Returns the sub-device whose receive was completed, or -1 if none was - in
 * which case the frame sits in the queue until NDIX posts its next XFRREN.
 * The caller uses that to decide whether to raise an interrupt: a completion
 * NDIX is never told about is a completion that never happened.
 */
int nd500_xmsg_frame_in_mem(const Nd500XmsgOps* ops,
                            const uint8_t* frame, uint32_t len) {
    int sub;

    if (!ops || !frame || len < 14 || len > XMSG_FRAME_MAX) return -1;

    if (g_rxq_count >= XMSG_RXQ_DEPTH) {
        /* The guest is not collecting. Drop the OLDEST rather than the newest:
         * on a network the fresher frame is nearly always the useful one, and
         * an ARP reply stuck behind five stale broadcasts helps nobody. */
        g_rxq_head = (g_rxq_head + 1) % XMSG_RXQ_DEPTH;
        g_rxq_count--;
        g_rx_dropped++;
        if (xmsgdbg())
            fprintf(stderr, "[XMSG] receive queue full - oldest frame dropped "
                            "(%lu dropped so far)\n", g_rx_dropped);
    }
    {
        int tail = (g_rxq_head + g_rxq_count) % XMSG_RXQ_DEPTH;
        memcpy(g_rxq[tail].buf, frame, len);
        g_rxq[tail].len = (uint16_t)len;
        g_rxq_count++;
    }

    /* Hand it to whoever is waiting. There is one ethernet interface being
     * served, so the first parked receive is the right one; the loop is over
     * sub-devices only so this does not have to change when there are two. */
    for (sub = 0; sub < XMSG_MAX_SUBDEV; sub++) {
        XmsgSub* s = &g_sub[sub];
        uint8_t cmd[XRING_CMD_SIZE];
        uint16_t T = (uint16_t)XMSG_XMSUX, D = 0, X = 0;

        if (!s->recv_parked) continue;

        memcpy(cmd, s->recv_cmd, sizeof cmd);
        s->recv_parked = 0;

        if (!xmsg_do_recv(ops, (uint16_t)sub,
                          nd500_xring_be16(cmd + CMD_ARG_A),
                          nd500_xring_be16(cmd + CMD_ARG_X),
                          nd500_xring_be16(cmd + CMD_ARG_D),
                          cmd, &T, &D, &X)) {
            /* Parked again - the frame did not fit and was dropped, so there
             * is still nothing to give it. The command stays outstanding,
             * which is right, and no interrupt is raised. */
            continue;
        }
        if (!xmsg_put_response(ops, cmd, T, 0, D, X)) return -1;
        if (xmsgdbg())
            fprintf(stderr, "[XMSG]   parked receive COMPLETED on subdev %d, "
                            "%u bytes\n", sub, X);
        return sub;
    }
    return -1;
}

int nd500_xmsg_frame_in(Nd500Cpu* cpu, const uint8_t* frame, uint32_t len) {
    Nd500XmsgOps ops;
    int sub;

    if (!cpu) return -1;
    ops.ring.read8  = xm_read8;
    ops.ring.write8 = xm_write8;
    ops.ring.ctx    = cpu;
    ops.pread8      = xm_pread8;
    ops.pwrite8     = xm_pwrite8;
    ops.ctx         = cpu;
    ops.frame_out   = g_uplink_fn;
    ops.frame_ctx   = g_uplink_ctx;

    sub = nd500_xmsg_frame_in_mem(&ops, frame, len);
    if (sub < 0) return sub;

    /* THE ONE NEW PIECE OF MACHINERY IN THE WHOLE RECEIVE PATH.
     *
     * Every XMSG answer so far has ridden home on the completion interrupt the
     * async FE_DCTL already posts - NDIX kicked us, so NDIX was already going
     * to be interrupted. A frame arriving from outside is nobody's completion:
     * without this, the response sits in the ring and xgintr() is never run to
     * find it, and the guest waits for a packet that is already in its own
     * memory.
     *
     * The queue and the delivery are the existing ones. nd500_fe_int_post
     * queues it, cpu_step delivers at the next safe instruction boundary, and
     * the IPL comes from what generic 7 asked for at FE_IDEV time (IPL_XM,
     * xg.c:128) - so this is the same road every other device already takes.
     * rpk is 0 because xgintr() does not read it: it drains the response ring
     * and works from what it finds there (xg.c:363-380). */
    nd500_fe_int_post(cpu, XMSG_GENERIC, (uint32_t)sub, 0);
    return sub;
}

int nd500_xmsg_service_mem(const Nd500XmsgOps* ops) {
    const Nd500XRingMem* mem;
    Nd500XRing cmd;
    uint8_t centry[XRING_CMD_SIZE];
    int answered = 0;

    if (!ops) return 0;
    mem = &ops->ring;

    nd500_xring_cmd_init(&cmd);

    /* DRAIN TO EMPTY. NDIX kicks only on the empty -> non-empty transition
     * (xg.c:484 `if (oldp == xmsg_cmd_buf.k)`), so anything left behind here is
     * never announced again and the link stops dead with nothing printed. */
    while (nd500_xring_get(mem, &cmd, centry)) {
        uint16_t subdev = nd500_xring_be16(centry + CMD_SUBDEV);
        uint16_t func   = nd500_xring_be16(centry + CMD_FUNC);
        uint16_t T = (uint16_t)XMSG_XENIM;   /* default answer: not implemented */
        uint16_t A = 0, D = 0, X = 0;
        int reply_now = 1;   /* cleared when a receive is parked instead */

        switch (func & XMSG_FUNC_MASK) {
        case XMSG_XFOPN:
            xmsg_do_open(subdev, &T, &A);
            break;
        case XMSG_XFGET:
            /* A carries the size. xma() is (T, A, X, D) while the struct is
             * T, A, D, X, so reading if_et.c:347 left to right is safe HERE
             * only because the other two arguments are 0 - do not generalise
             * from it. Measured arriving as A=0x05F0 = 1520. */
            xmsg_do_get(subdev, nd500_xring_be16(centry + CMD_ARG_A), &T, &A);
            break;
        case XMSG_XFWRI:
            /* A = buffer (ND-100 word address), D = length. NOT A and X: xma()
             * takes (T, A, X, D) but the struct is T, A, D, X. */
            xmsg_do_wri(ops, subdev,
                        nd500_xring_be16(centry + CMD_ARG_A),
                        nd500_xring_be16(centry + CMD_ARG_D), &T);
            break;
        case XMSG_XFREL:
            xmsg_do_rel(subdev, &T);
            break;
        case XMSG_XFSND:
            /* The port is in X, not D: xma() is (T, A, X, D) and the struct is
             * T, A, D, X, so if_et.c:393's third argument lands in X. Measured
             * arriving as func=02014 X=0x0001. */
            xmsg_do_snd(subdev, func, nd500_xring_be16(centry + CMD_ARG_X), &T);
            break;
        case XMSG_XFRRE:
        case XMSG_XFRREN:
        case XMSG_XFRRH:
        case XMSG_XFRCV:
            /* A = port, X = buffer word address, D = buffer size (if_et.c:398).
             * All four are what xg.c's is_receive() calls a receive (xg.c:335),
             * so all four must clear HAS_RCV and not HAS_OTHER - which they do,
             * because the func is echoed back whole. */
            reply_now = xmsg_do_recv(ops, subdev,
                                     nd500_xring_be16(centry + CMD_ARG_A),
                                     nd500_xring_be16(centry + CMD_ARG_X),
                                     nd500_xring_be16(centry + CMD_ARG_D),
                                     centry, &T, &D, &X);
            break;
        case XMSG_XFMST:
            xmsg_do_mst(subdev, &T, &A, &D);
            break;
        case XMSG_XETHER:
            /* A = buffer, X = length, D = port (if_et.c:539). The response's
             * func must come back as XETHER or etintr() sends it to the wrong
             * handler (if_et.c:570-576) - which it does, because func is
             * echoed whole. */
            xmsg_do_ether(ops, subdev,
                          nd500_xring_be16(centry + CMD_ARG_A),
                          nd500_xring_be16(centry + CMD_ARG_D),
                          nd500_xring_be16(centry + CMD_ARG_X), &T);
            break;
        default:
            /* Answered, not ignored. An unanswered command wedges the
             * sub-device for good (see the header); an XENIM answer retires
             * xgdctl's HAS_RCV/HAS_OTHER flag and gives if_et.c's eterror()
             * something to print. */
            break;
        }

        /* A parked receive has no answer yet, by design. Keep draining - the
         * kick only comes on the empty -> non-empty transition, so stopping
         * here would strand every command behind it. */
        if (!reply_now)
            continue;

        if (!xmsg_put_response(ops, centry, T, A, D, X))
            break;
        answered++;

        if (xmsgdbg()) {
            /* Every field of the command, not just the ones we act on. The
             * argument order is a known trap: xma() is declared (T, A, X, D)
             * while struct xmsg_args is laid out T, A, D, X (if_et.c:1167),
             * so reading a call site left to right swaps D and X. Printing the
             * bytes as they actually ARRIVE is the only way not to be fooled -
             * and the addresses in A/X are ND-100 WORD addresses squeezed
             * through a `short`, which is worth seeing before trusting. */
            fprintf(stderr, "[XMSG] cmd seq=%u subdev=%u func=0%o "
                            "args T=0%o A=0x%04X D=0x%04X X=0x%04X magno=0x%08X"
                            "  -> T=%d A=%u\n",
                    nd500_xring_be32(centry + CMD_SEQWORD) >> 1, subdev, func,
                    nd500_xring_be16(centry + CMD_ARG_T),
                    nd500_xring_be16(centry + CMD_ARG_A),
                    nd500_xring_be16(centry + CMD_ARG_D),
                    nd500_xring_be16(centry + CMD_ARG_X),
                    nd500_xring_be32(centry + CMD_MAGNO),
                    (int16_t)T, A);
        }
    }

    return answered;
}
