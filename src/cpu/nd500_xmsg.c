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
typedef struct XmsgSub {
    uint32_t datbuf_word;  /* full-width dton(&xdata[sub]) from FE_OPEN, 0=unset */
    uint16_t port;         /* 0 = no port open (XFOPN not yet done)          */
    int      msg_open;     /* XFGET done and not yet XFREL                   */
    uint16_t msg_size;     /* the size XFGET asked for                       */
    uint16_t msg_len;      /* bytes written into it so far by XFWRI          */
    uint8_t  msg[XMSG_MSG_MAX];
} XmsgSub;

static XmsgSub g_sub[XMSG_MAX_SUBDEV];
static uint16_t g_next_port = 1;                    /* port 0 is not handed out */

void nd500_xmsg_reset(void) {
    memset(g_sub, 0, sizeof g_sub);
    g_next_port = 1;
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

/* ---- the service loop ---------------------------------------------------- */

int nd500_xmsg_service(Nd500Cpu* cpu) {
    Nd500XmsgOps ops;
    if (!cpu) return 0;
    ops.ring.read8  = xm_read8;
    ops.ring.write8 = xm_write8;
    ops.ring.ctx    = cpu;
    ops.pread8      = xm_pread8;
    ops.pwrite8     = xm_pwrite8;
    ops.ctx         = cpu;
    return nd500_xmsg_service_mem(&ops);
}

int nd500_xmsg_service_mem(const Nd500XmsgOps* ops) {
    const Nd500XRingMem* mem;
    Nd500XRing cmd, resp;
    uint8_t centry[XRING_CMD_SIZE];
    uint8_t rentry[XRING_RESP_SIZE];
    int answered = 0;

    if (!ops) return 0;
    mem = &ops->ring;

    nd500_xring_cmd_init(&cmd);
    nd500_xring_resp_init(&resp);

    /* DRAIN TO EMPTY. NDIX kicks only on the empty -> non-empty transition
     * (xg.c:484 `if (oldp == xmsg_cmd_buf.k)`), so anything left behind here is
     * never announced again and the link stops dead with nothing printed. */
    while (nd500_xring_get(mem, &cmd, centry)) {
        uint16_t subdev = nd500_xring_be16(centry + CMD_SUBDEV);
        uint16_t func   = nd500_xring_be16(centry + CMD_FUNC);
        uint16_t T = (uint16_t)XMSG_XENIM;   /* default answer: not implemented */
        uint16_t A = 0, D = 0, X = 0;

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
        default:
            /* Answered, not ignored. An unanswered command wedges the
             * sub-device for good (see the header); an XENIM answer retires
             * xgdctl's HAS_RCV/HAS_OTHER flag and gives if_et.c's eterror()
             * something to print. */
            break;
        }

        /* Build the response. seq, subdev and func are echoed VERBATIM from the
         * command - xgintr() picks which outstanding-request flag to clear from
         * the response's own func (xg.c:374), and a mismatch there wedges the
         * sub-device silently. */
        memset(rentry, 0, sizeof rentry);
        memcpy(rentry + RESP_SEQWORD, centry + CMD_SEQWORD, 4);
        nd500_xring_put_be16(rentry + RESP_SUBDEV, subdev);
        nd500_xring_put_be16(rentry + RESP_FUNC,   func);
        nd500_xring_put_be16(rentry + RESP_ARG_T,  T);
        nd500_xring_put_be16(rentry + RESP_ARG_A,  A);
        nd500_xring_put_be16(rentry + RESP_ARG_D,  D);
        nd500_xring_put_be16(rentry + RESP_ARG_X,  X);
        nd500_xring_put_be16(rentry + RESP_CBA,    0);

        if (!nd500_xring_put(mem, &resp, rentry)) {
            /* The response ring is full: 113 answers are outstanding and NDIX
             * has not run xgintr() once. That cannot happen while the guest is
             * healthy, and dropping the answer would wedge the sub-device, so
             * say so loudly rather than lose it quietly. */
            fprintf(stderr, "[XMSG] response ring FULL - answer to subdev=%u "
                            "func=0%o DROPPED (that sub-device is now stuck)\n",
                    subdev, func);
            break;
        }
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
