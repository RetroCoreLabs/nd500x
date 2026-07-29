/*
 * nd500_fecall.h - public interface to the ND-100 front-end call emulation.
 *
 * Only the guest TERMINAL (tty) plumbing is declared here; the rest of the
 * fecall machinery is internal to nd500_fecall.c.
 *
 * The NDIX guest talks to its terminals through io/mx.c, a multiplexor keyed
 * by UNIT number: the fecall device word is (generic << 16) | unit, generic 3
 * is TERM_IN and generic 4 is TERM_OUT. Unit 0 is /dev/console. Input for all
 * units shares ONE guest ring (mx_bin); each element is (unit << 8) | char,
 * so the unit travels with the byte.
 *
 * Host side:
 *   - nd500_fecall_tty_input()  queues bytes for a given unit. Any thread may
 *     call it; the CPU thread drains the queue into the guest ring.
 *   - nd500_fecall_set_tty_output() registers a sink that receives the bytes
 *     the guest writes to that unit. The bytes handed to the sink have already
 *     been through the console bit-7 mask, exactly like the stdout copy.
 */
#ifndef ND500_FECALL_H
#define ND500_FECALL_H

#include <stdint.h>

/* Highest unit number the host side tracks. Units above this still reach the
 * guest ring, they just cannot have a host sink attached. */
#define ND500_TTY_MAX_UNITS 256

/* Queue host bytes as input for guest tty <unit>. */
void nd500_fecall_tty_input(int unit, const char* buf, int len);

/* Backwards-compatible shorthand for unit 0 (/dev/console). */
void nd500_fecall_console_input(const char* buf, int len);

/* Sink for guest output on one unit. buf/len is a chunk of one FE_WRIT. */
typedef void (*Nd500TtyOutFunc)(int unit, const unsigned char* buf, int len, void* ctx);

/* Attach (fn != NULL) or detach (fn == NULL) the host sink for <unit>.
 * While a sink is attached to a unit != 0 the stdout copy of that unit's
 * output is suppressed, so a remote terminal does not scribble over the local
 * console log. Unit 0 always keeps its stdout copy (see nd500x --telnet). */
void nd500_fecall_set_tty_output(int unit, Nd500TtyOutFunc fn, void* ctx);

#endif /* ND500_FECALL_H */
