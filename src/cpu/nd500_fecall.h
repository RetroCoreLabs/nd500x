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

/* --- deferred clean shutdown -------------------------------------------- */

/* Declared rather than included: this header is pulled in by the frontend,
 * which has no business seeing the whole CPU definition. */
struct Nd500Cpu;

/* Ask for the guest to shut itself down at the next SAFE point, rather than
 * right now. Safe means: kernel domain, and a current process the kernel is
 * willing to sleep - which is exactly what the _syscall dispatcher entry gives
 * us, since a process only gets there by trapping in from user mode.
 *
 * Injecting immediately does not work and was measured failing both ways: at an
 * idle prompt the shell is SSLEEP, so boot() -> update() -> sleep() hits
 * kern_synch.c's "chan==0 || p_stat != SRUN" test and panics; mid-command the
 * CPU is in user domain and vectoring it at a kernel address just kills that
 * process ("Memory fault - core dumped").
 *
 * boot_addr is _boot and syscall_addr is _syscall; both are passed in because
 * symbol lookup lives in ndlib, above this layer. Returns 0 if the request was
 * accepted. nd500_ndix_halt_check() then does the work from the CPU loop. */
int  nd500_ndix_request_halt(uint32_t boot_addr, uint32_t syscall_addr);

/* Non-zero while a halt is armed and has not fired yet. */
int  nd500_ndix_halt_pending(void);

/* Non-zero once the guest has shut ITSELF down - it reached FE_EXIT, so the
 * kernel's boot() finished syncing and halted.
 *
 * This is deliberately different from "the machine is not running". A
 * breakpoint, a trap or an F12 "exit now" all clear run_flag too, and after
 * those the debugger prompt is exactly what is wanted. After a clean shutdown
 * it is not: there is no longer a guest to debug, and the session should end. */
int  nd500_ndix_guest_exited(void);

/* Called from the disk-write path so a pending shutdown can tell when the
 * flush it asked for has actually finished. */
void nd500_ndix_halt_note_write(void);

/* Called from the instruction loop with the PC about to execute. Performs the
 * injection when the CPU has reached the safe point; a no-op otherwise. */
void nd500_ndix_halt_check(struct Nd500Cpu* cpu, uint32_t pc);

/* Backwards-compatible shorthand for unit 0 (/dev/console). */
void nd500_fecall_console_input(const char* buf, int len);

/* Which guest tty the LOCAL terminal is attached to - what stdin feeds and what
 * stdout shows. Unit 0 (the console) until the F12 menu moves it. Changing it
 * is invisible to the guest: all four ttys keep running either way. */
void nd500_fecall_set_local_unit(int unit);
int  nd500_fecall_local_unit(void);

/* Sink for guest output on one unit. buf/len is a chunk of one FE_WRIT. */
typedef void (*Nd500TtyOutFunc)(int unit, const unsigned char* buf, int len, void* ctx);

/* Attach (fn != NULL) or detach (fn == NULL) the host sink for <unit>.
 * While a sink is attached to a unit != 0 the stdout copy of that unit's
 * output is suppressed, so a remote terminal does not scribble over the local
 * console log. Unit 0 always keeps its stdout copy (see nd500x --telnet). */
void nd500_fecall_set_tty_output(int unit, Nd500TtyOutFunc fn, void* ctx);

#endif /* ND500_FECALL_H */
