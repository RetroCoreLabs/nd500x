#pragma once
#include <stdint.h>
#include "../machine/machine_types.h"

int nd500_debugger_repl(Nd500Machine* m);
#ifdef WITH_DEBUGGER
int nd500_dap_start(Nd500Machine* m, int port);
#endif

/* Domain tracking for debugger - call this when loading a domain from any code path */
void nd500_debugger_register_domain(uint8_t domain_num, const char* name,
                                    const char* filepath, uint32_t entry,
                                    uint32_t tha, int seg_count);


