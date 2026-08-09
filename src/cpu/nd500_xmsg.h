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
#define XMSG_XFOPN   012   /* open a port  */
#define XMSG_XFCLS   013   /* close a port */
#define XMSG_FUNC_MASK 0xff

/* XMSG status codes returned in args.T (if/xmsg.h:128-161, 179).
 * XMSUX is 0 - success. Everything negative is an error, and if_et.c tests
 * exactly `xa->T < 0`. */
#define XMSG_XMSUX    0     /* success                            */
#define XMSG_XENIM   -5     /* facility not yet implemented       */
#define XMSG_XENOP  -011    /* no more ports available (octal -9) */

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
int nd500_xmsg_service_mem(const Nd500XRingMem* mem);

/*
 * Forget all port allocations. Called from the NDIX boot path when the ring
 * headers are (re)initialised, so a second boot in the same process does not
 * inherit the first one's ports.
 */
void nd500_xmsg_reset(void);

#ifdef __cplusplus
}
#endif

#endif /* ND500_XMSG_H */
