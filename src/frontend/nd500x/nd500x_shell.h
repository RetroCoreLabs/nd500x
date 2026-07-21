/*
 * nd500x_shell.h - SINTRAN-flavoured interactive shell (--monitor mode).
 *
 * A faithful-surface reimplementation of the SINTRAN III command prompt: an
 * "@" prompt you log in to, run DOM programs from by name, and log out of.
 * Command surface and behaviour are sourced from the ND manuals - see
 * /home/ronny/repos/nd500x/docs/SINTRAN-SHELL-SPEC.md.
 *
 * Phase 1 (this file): local terminal, login/logout/help/exit, list-files,
 * set-/get-terminal-type, and run-a-DOM-by-name (RECOVER-DOMAIN). Telnet
 * transport and scripted (--script) input are added in a later phase.
 */
#ifndef ND500X_SHELL_H
#define ND500X_SHELL_H

#include "../../machine/machine_protos.h"
#include "../../cpu/cpu_protos.h"

/*
 * Run the interactive shell REPL until the user exits (LOGOUT from a
 * logged-out state, or EXIT). The machine, CPU and libmon must already be
 * initialised by the caller. Returns 0 on normal exit.
 *
 * script_path: optional path to a file of commands to feed before returning to
 * interactive input (batch mode); NULL for pure interactive.
 */
/* telnet_port > 0 serves the shell over TCP/telnet (waits for a client);
 * 0 uses the local terminal. */
int nd500x_shell_run(Nd500Machine* machine, Nd500Cpu* cpu, const char* script_path,
                     int telnet_port);

#endif /* ND500X_SHELL_H */
