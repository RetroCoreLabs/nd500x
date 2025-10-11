#pragma once
#include <stdint.h>
#include "../machine/machine_types.h"

int nd500_debugger_repl(Nd500Machine* m);
#ifdef WITH_DEBUGGER
int nd500_dap_start(Nd500Machine* m, int port);
#endif


