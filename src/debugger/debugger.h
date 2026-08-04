#pragma once
#include <stdint.h>
#include "../machine/machine_types.h"

int nd500_debugger_repl(Nd500Machine* m);

/* When the guest console also lives somewhere other than stdin (a telnet
 * terminal), EOF on stdin is just "nobody is typing here any more" - it must
 * NOT be turned into an EOT for the guest, which would close a shell the
 * remote user is still holding. Call with 1 to suppress that EOT. */
void nd500_debugger_set_stdin_eof_quiet(int quiet);
/* Suppress the debugger's banner/readline chrome (used by --ndix). */
void nd500_debugger_set_quiet_banner(int quiet);

/* A hot key that belongs to the EMULATOR rather than to the guest.
 *
 * While the guest owns the terminal every keystroke is forwarded to it, so
 * without a hook like this there is no way to reach the emulator except by
 * killing it. The handler is offered each raw read before the guest sees it and
 * answers with what it did:
 *
 *    0    not my key - give the bytes to the guest
 *   >0    consumed that many leading bytes; the rest still goes to the guest
 *   -1    consumed everything and the session should END (the user chose to
 *         shut the machine down)
 *   -2    these bytes are the START of my key but incomplete - hold them and
 *         read more. A terminal is free to split an escape sequence across two
 *         read()s, and guessing wrong would leak a stray ESC to the guest.
 *
 * Registered by the frontend (nd500x installs the F12 menu); the debugger keeps
 * working with no handler at all, which is what the other frontends do. */
typedef int (*Nd500GuestKeyFn)(Nd500Machine* m, const char* buf, int len);
void nd500_debugger_set_guest_key_handler(Nd500GuestKeyFn fn);
#ifdef DAP_ENABLED
struct DAPServer;
int nd500_dap_start(Nd500Machine* m, int port);
int nd500_dap_stop(void);
int nd500_dap_is_active(void);
/* Register callbacks on a server without starting transport/thread
 * (unit tests drive callbacks directly through the server struct) */
int nd500_dap_bind(Nd500Machine* m, struct DAPServer* server);
#endif

/* Domain tracking for debugger - call this when loading a domain from any code path */
void nd500_debugger_register_domain(uint8_t domain_num, const char* name,
                                    const char* filepath, uint32_t entry,
                                    uint32_t tha, int seg_count);


