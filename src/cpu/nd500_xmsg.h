/*
 * nd500_xmsg.h - the ND-100 side of NDIX's XMSG interface.
 *
 * WHAT THIS IS
 * ------------
 * nd500_xring.c owns the ring DISCIPLINE (empty/full/put/get). This file owns
 * the SERVER that sits behind it: it takes the commands NDIX puts in the
 * command ring, decides what the answer is, and puts responses in the response
 * ring. On a real machine this work is done by the XMSG kernel over on the
 * ND-100; here there is no ND-100, so we answer for it.
 *
 * HOW NDIX DRIVES IT (if/xg.c)
 * ----------------------------
 *   R_put()  fills a command slot, advances p, and - ONLY when the ring was
 *            empty beforehand - issues an async FE_DCTL with request=DCTL_KICK
 *            on generic device 7. That kick is our one and only "you have mail"
 *            signal, which is why the server must DRAIN TO EMPTY every time it
 *            runs. Stop with entries still in the ring and no further kick ever
 *            comes: the link stalls with nothing printed anywhere.
 *   xgintr() is called from the interrupt dispatcher for generic 7
 *            (GENERIC/ioconf.c:100 drvtab[6] = { xgattach, xgintr }), drains the
 *            response ring with R_get(), clears the per-subdevice HAS_RCV /
 *            HAS_OTHER flag according to the response's OWN func field, and
 *            calls the subdevice handler registered at xgopen() time.
 *
 * THE WEDGE HAZARD - read this before adding a command
 * ----------------------------------------------------
 * xgintr() decides which outstanding-request flag to clear from the RESPONSE's
 * func (xg.c:374). If a response carries a func that does not match the command
 * it answers, the wrong flag is cleared: the real outstanding request is never
 * retired, the next xgdctl() on that subdevice returns EBUSY for ever, and
 * NOTHING is reported - no panic, no error line, no counter. So every response
 * built here echoes seq, subdev and func straight back out of the command it
 * answers. Do not "improve" that.
 *
 * WHAT IS ANSWERED TODAY
 * ----------------------
 *   XFOPN (012)  open a port - answered properly, with a port number in A.
 *   everything else - answered with XENIM ("facility not yet implemented") so
 *                     the caller gets an error it can print rather than a hang.
 * An error answer is always better than no answer: xgdctl's flag is retired
 * either way, and if_et.c's eterror() prints the function and the status.
 *
 * SOURCE OF TRUTH
 *   NDIX kernel: if/xg.c, if/xmsg.h, if/if_et.c (etattach/etintr),
 *                machine/if.h (DCTL_KICK=1, DCTL_WAIT=2, XMSG generic = 7)
 *   Written up : notes/docs/REFERENCE_XMSG_ADDRESSES_2026-08-09.md and
 *                notes/docs/SPEC_ENUM0_ETHERNET_MEDIA_SERVER_FOR_ND500X_2026-08-08.md
 *                in the NDIX-C repository.
 */

#ifndef ND500_XMSG_H
#define ND500_XMSG_H

#include <stdint.h>
#include "cpu_protos.h"
#include "nd500_xring.h"

#ifdef __cplusplus
extern "C" {
#endif

/* FE_DCTL request codes on generic 7 (struct _dctl_cpk_xmsg, machine/if.h:364).
 * Prefixed because nd500_fecall.c already has a DCTL_ set for TAPE where 1 and
 * 2 mean forward/backward space file - the same numbers, a different device. */
#define XMSG_DCTL_KICK     1   /* commands are waiting in the command ring   */
#define XMSG_DCTL_WAIT     2   /* wake me when there is command-ring space   */
#define XMSG_DCTL_INUSE    3   /* which ethernet interface NDIX is using     */
#define XMSG_DCTL_RESTART  4   /* stop and restart the interfacing tool      */
#define XMSG_DCTL_STOP     5   /* stop the ethernet interface                */

/* The XMSG function codes we care about (if/xmsg.h:78-97). The low byte is the
 * function; the high bits are option flags (XFMASK = 0xff strips them). */
#define XMSG_XFGET   002   /* get message space, size in A     */
#define XMSG_XFREL   003   /* release message space            */
#define XMSG_XFWRI   007   /* write from a buffer into the message */
#define XMSG_XFMST   011   /* get message status -> magic number   */
#define XMSG_XFOPN   012   /* open a port                      */
#define XMSG_XFCLS   013   /* close a port                     */
#define XMSG_XFSND   014   /* send the message to a port       */
#define XMSG_XFRRE   051   /* receive and read message         */
#define XMSG_XFRREN  060   /* receive and read, do not wait    */
#define XMSG_XETHER  055   /* the special ethernet call        */
#define XMSG_XFRRH   052   /* receive header only - counts as a receive */
#define XMSG_XFRCV   050   /* receive - counts as a receive             */
#define XMSG_FUNC_MASK 0xff

/* Option bits OR'd into the function (if/xmsg.h:100-112). We never act on them
 * - the function is echoed back whole - but they are named here because a trace
 * line reads as gibberish without them: the attach XFSND arrives as 02014,
 * which is XFROU|XFSND, and the blocking receive as 0100051 = XFWTF|XFRRE. */
#define XMSG_XFWTF   0x8000  /* wait until complete (XFRRE)          */
#define XMSG_XFWAK   0x8000  /* do not block the task (XFRREN)       */
#define XMSG_XFRMR   0x1000  /* release the message after read       */
#define XMSG_XFROU   0x0400  /* XFSND: send to the local XROUT       */

/* XROUT service requests, carried in xr_header.xh_service (if/xmsg.h:117).
 * OCTAL 0101 = 65 decimal = 0x41 - which is what the wire shows. */
#define XMSG_XSLET   0101    /* "send a letter"                      */

/* Ethernet access-protocol message types, EXMHDtype (if/if_access.h:128-135).
 * EXMversion is 128, so these are 128+n and all of them look like 00 8x on the
 * wire. */
#define XMSG_EXMTYdata         128
#define XMSG_EXMTYattach       129
#define XMSG_EXMTYstatus       130
#define XMSG_EXMTYdetach       131
#define XMSG_EXMTYdefineMulti  132

/* EXMSTstatus values (if/if_access.h:141). 0 is a good attach or detach. */
#define XMSG_EXMATok           0
#define XMSG_EXMATillegal      1

/* XMSG status codes returned in args.T (if/xmsg.h:128-161, 179).
 * XMSUX is 0 - success. Everything negative is an error, and if_et.c tests
 * exactly `xa->T < 0`. */
#define XMSG_XMSUX    0     /* success                                    */
#define XMSG_XENIM   -5     /* facility not yet implemented               */
#define XMSG_XENOP  -011    /* no more ports available                    */
#define XMSG_XEXBF  -033    /* message already has an XMSG buffer (XFDUB) */
#define XMSG_XENDP  -035    /* no port open, so the port param is invalid */
#define XMSG_XEILM  -025    /* illegal message size                       */
#define XMSG_XENDM  -013    /* no default (current) message               */
#define XMSG_XEITL  -036    /* illegal transfer length for read/write     */

/*
 * Drain the command ring, answer every command in it, and leave the answers in
 * the response ring. Returns the number of commands answered.
 *
 * Call this from the DCTL_KICK handler. The completion interrupt that the async
 * FE_DCTL already posts is what gets xgintr() run, so the responses are picked
 * up without this file raising an interrupt of its own.
 */
int nd500_xmsg_service(Nd500Cpu* cpu);

/*
 * The same thing over a bare byte accessor, with no CPU anywhere in it.
 *
 * This is the part that is worth testing, and it is split out for the same
 * reason nd500_xring.c has no CPU in it: every way of getting a response wrong
 * presents as "nothing happened" inside a running guest, so it has to be
 * checkable against a plain array rather than by booting and squinting.
 * nd500_xmsg_service() is a two-line wrapper that supplies the guest-memory
 * accessor and calls this.
 */
/*
 * What the server needs to reach, in the two different address spaces it has
 * to deal with:
 *
 *   ring   - the two ring buffers, at ND-500 VIRTUAL addresses in segment 6.
 *   pread8 - PHYSICAL memory. Every buffer address inside an xmsg_args is an
 *            ND-100 WORD address produced by dton() (h/types.h:56), and the
 *            conversion is `phys = word * 2 - FE_PRIVATE` - the same one the
 *            tty path already does (nd500_fecall.c:545). It does not go through
 *            the ND-500 MMU at all, so it cannot share the ring accessor.
 *
 * Split this way so the whole server stays checkable against two plain arrays.
 */
/*
 * Where a transmitted frame goes. This is the whole of the server's outside
 * world in the outgoing direction - "frame_out(bytes, len)" in the plan's
 * architecture section, and deliberately nothing more: no sockets, no files, no
 * threads, so the same code runs native, in wasm, and behind ndmonlib.
 *
 * `frame` is a plain ethernet frame - destination, source, type, payload. The
 * 6-byte XMSG datagram header (struct ac_head) has already been stripped.
 */
typedef void (*Nd500XmsgFrameOut)(void* ctx, const uint8_t* frame, uint32_t len);

typedef struct Nd500XmsgOps {
    Nd500XRingMem ring;
    uint8_t (*pread8) (void* ctx, uint32_t phys);
    void    (*pwrite8)(void* ctx, uint32_t phys, uint8_t val);
    void*   ctx;
    /* NULL is a working uplink - it drops the frame and says so. That is
     * "uplink_null", and it is what phases 1-4 need. */
    Nd500XmsgFrameOut frame_out;
    void*             frame_ctx;
} Nd500XmsgOps;

/*
 * Plug an uplink into the CPU-driven path (nd500_xmsg_service). Without one,
 * transmitted frames are logged under ND500X_FEDBG and dropped.
 */
void nd500_xmsg_set_uplink(Nd500XmsgFrameOut fn, void* ctx);

int nd500_xmsg_service_mem(const Nd500XmsgOps* ops);

/*
 * Forget all port allocations. Called from the NDIX boot path when the ring
 * headers are (re)initialised, so a second boot in the same process does not
 * inherit the first one's ports.
 */
void nd500_xmsg_reset(void);

/*
 * Record the full-width buffer base for a sub-device, from the FE_OPEN command
 * packet (`long datbuf` = dton(&xdata[sub]), xg.c:215, machine/if.h:173).
 *
 * WHY THIS IS NEEDED AT ALL. Every buffer address inside a command travels in
 * `struct xmsg_args`, whose fields are all `short` (if/xmsg.h:25-30), while
 * xma() takes them as `long` and assigns straight in (`xap->A = A`,
 * if_et.c:1173). A word address wider than 16 bits is therefore TRUNCATED
 * before it ever reaches this side. Measured: the attach letter really sat at
 * word 0x00151D1E and arrived as 0x1D1E.
 *
 * `datbuf` is the one full-width address the front end is given, so it supplies
 * the missing high bits. nd500_xmsg_full_word() completes a truncated address
 * against it.
 */
void nd500_xmsg_note_datbuf(uint32_t subdev, uint32_t datbuf_word);

/*
 * Complete a 16-bit truncated word address for a sub-device, using the base
 * recorded by nd500_xmsg_note_datbuf(). Exposed for the tests.
 *
 * It picks the candidate NEAREST to the base, so a buffer just below the base's
 * own 64K boundary still resolves - the alternative (masking in the base's high
 * bits unconditionally) is wrong by 128 KB whenever the two straddle it.
 */
uint32_t nd500_xmsg_full_word(uint32_t subdev, uint16_t truncated);

#ifdef __cplusplus
}
#endif

#endif /* ND500_XMSG_H */
