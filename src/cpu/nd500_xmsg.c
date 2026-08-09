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

/* ---- server state --------------------------------------------------------
 * Deliberately file-static, and single-machine. nd500x runs one Nd500Machine
 * per process everywhere it is used today (the native frontend, the debugger
 * and the wasm build all create exactly one), and nd500_fecall.c already keeps
 * per-run state the same way. nd500_xmsg_reset() is called from the boot path
 * so a second boot in one process starts clean rather than inheriting ports. */
static uint16_t g_port_of_subdev[XMSG_MAX_SUBDEV];  /* 0 = no port open */
static uint16_t g_next_port = 1;                    /* port 0 is not handed out */

void nd500_xmsg_reset(void) {
    memset(g_port_of_subdev, 0, sizeof g_port_of_subdev);
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
    if (g_port_of_subdev[subdev] == 0)
        g_port_of_subdev[subdev] = g_next_port++;
    *out_T = (uint16_t)XMSG_XMSUX;       /* 0 - if_et.c tests `xa->T < 0` */
    *out_A = g_port_of_subdev[subdev];
}

/* ---- the service loop ---------------------------------------------------- */

int nd500_xmsg_service(Nd500Cpu* cpu) {
    Nd500XRingMem mem;
    if (!cpu) return 0;
    mem.read8 = xm_read8;
    mem.write8 = xm_write8;
    mem.ctx = cpu;
    return nd500_xmsg_service_mem(&mem);
}

int nd500_xmsg_service_mem(const Nd500XRingMem* mem_in) {
    const Nd500XRingMem* mem = mem_in;
    Nd500XRing cmd, resp;
    uint8_t centry[XRING_CMD_SIZE];
    uint8_t rentry[XRING_RESP_SIZE];
    int answered = 0;

    if (!mem) return 0;

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

        if (xmsgdbg())
            fprintf(stderr, "[XMSG] cmd subdev=%u func=0%o -> T=%d A=%u\n",
                    subdev, func, (int16_t)T, A);
    }

    return answered;
}
