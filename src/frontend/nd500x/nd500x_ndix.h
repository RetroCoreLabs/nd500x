/*
 * nd500x_ndix.h - NDIX boot setup (--ndix) and the guest-tty telnet bridge.
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 *
 * --ndix absorbs what the run-ndix.sh wrapper used to do: point the front-end
 * call layer at a root disk image, set the environment defaults the NDIX boot
 * needs, find the kernel image and hand the debugger a "load vmunix" + "run"
 * script. Every path comes from the command line or is derived from it; none
 * is compiled in.
 *
 * The telnet bridge registers NDIX tty units with the terminal server copied
 * from nd100x, so a remote client can be a guest terminal.
 */
#ifndef ND500X_NDIX_H
#define ND500X_NDIX_H

/* Resolve paths, export the NDIX environment defaults and chdir to the kernel
 * directory. <image> is the root disk image; <kernel> may be NULL (then it is
 * derived, see the .c file); <root_opt> is an explicit --sintran-root or NULL.
 * On success *load_cmd receives the debugger "load ..." argument to use.
 * Returns 0 on success, -1 with a message on stderr otherwise. */
int nd500x_ndix_setup(const char* image, const char* kernel, const char* root_opt,
                      char* load_cmd, int load_cmd_len);

/* Start the terminal server on <port> and register the NDIX guest ttys.
 * Returns 0 on success, -1 on error. */
struct Nd500Machine;
/* Non-zero when no <kernel>.init exists, so --ndix must do the boot setup itself. */
int nd500x_ndix_autoboot_needed(void);
/* Run that setup, deriving load addresses from the .pseg/.dseg files. */
int nd500x_ndix_autoboot(struct Nd500Machine* m,
                         int (*run)(struct Nd500Machine*, const char*, void*),
                         void* ctx);

/* Ask the GUEST to shut itself down: vector it into the kernel's own boot()
 * halt path, which syncs the buffer cache ("syncing disks... done") and then
 * issues the FE_EXIT fecall that stops the machine. Returns 0 when the guest
 * was vectored - the caller must keep RUNNING it, the stop arrives via FE_EXIT
 * - and -1 when it could not be done, in which case only an unclean exit is
 * left. Does NOT clear run_flag itself. */
int nd500x_ndix_halt_guest(struct Nd500Machine* m);

/* Serve the first <count> guest ttys - console, tty01..tty08, in that order.
 * count <= 0 means all of them; a count above the number available is reported
 * and clamped, never silently accepted.
 *
 * ONE port serves them all: a connecting client is shown a menu and picks its
 * terminal (telnetserver.c, copied from nd100x). There is no port per tty. */
int nd500x_ndix_telnet_start(int port, int count);

/* Stop the terminal server (safe if it was never started). */
void nd500x_ndix_telnet_stop(void);

/* True once the terminal server is running. */
int nd500x_ndix_telnet_active(void);

/* ---- status and control, for the F12 menu -------------------------------
 *
 * Thin wrappers over the TelnetServer so the menu needs neither its types nor
 * its pointer. All are safe to call while the server is stopped: the count is
 * 0 and the rest do nothing.
 *
 * Modelled on what nd100x's menu offers - see which lines are in use and from
 * where, and hang one up. */

/* TCP port being listened on, or 0 when the server is not running. */
int nd500x_ndix_telnet_port(void);

/* How many terminals the running server offers (0 when stopped). */
int nd500x_ndix_telnet_count(void);

/* Details of terminal <idx> (0-based, in the order the server lists them).
 *   name      - receives the tty name, e.g. "tty01"
 *   connected - non-zero while a telnet client holds it
 *   addr      - receives "IP:port" of that client, or "" when free
 * Returns 0 on success, -1 if idx is out of range or the server is stopped. */
int nd500x_ndix_telnet_info(int idx, const char** name, int* connected,
                            char* addr, int addrlen);

/* Hang up whatever client holds terminal <idx>, AND log the guest session on
 * that line out, so the next person to take it gets a login prompt rather than
 * somebody else's shell. Returns 0 if a client was disconnected, -1 otherwise.
 *
 * The logout is done by sending INTR then EOF to the line, because a carrier
 * drop - what real hardware uses - is ignored on a soft-carrier line, which
 * every local tty is. See the implementation for the driver code that says so,
 * and for what this therefore cannot dislodge. */
int nd500x_ndix_telnet_disconnect(int idx);

/* Clients that have connected but not yet chosen a terminal from the menu. */
int nd500x_ndix_telnet_pending(void);

/* Tell the server which line the LOCAL window is attached to, so it stops
 * offering that one to telnet clients. Exactly one line is marked at a time;
 * every other is cleared, so switching away with F12 releases the old line.
 *
 * Without this a telnet client could pick the console straight out of the menu
 * and end up sharing it with the local terminal. Returns 0 if the unit matched
 * a served terminal, -1 otherwise (including when the server is stopped). */
int nd500x_ndix_telnet_mark_local(int unit);

/* Non-zero while a telnet client holds terminal <idx>. Used by the F12 menu to
 * refuse switching the local window onto a line somebody else is using. */
int nd500x_ndix_telnet_in_use(int idx);

/* Who is logged in on <ttyname> ("console", "tty01", ...), from /etc/utmp in
 * the disk image. Writes the user name into <out> and returns 0; returns -1
 * when nobody is logged in there, or the file cannot be read.
 *
 * This reads a FILE from the image - the same path used to extract the kernel
 * at boot - not the running kernel: no symbols and no guest memory are touched.
 * The consequence is that a login appears only once utmp has reached the disk
 * through the buffer cache, so a very fresh one can be missing briefly. */
int nd500x_ndix_utmp_user(const char* ttyname, char* out, int outlen);

#endif /* ND500X_NDIX_H */
