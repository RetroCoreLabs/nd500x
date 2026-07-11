#pragma once
#include <stdint.h>
#include "../machine/machine_types.h"

int nd500_debugger_repl(Nd500Machine* m);
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


