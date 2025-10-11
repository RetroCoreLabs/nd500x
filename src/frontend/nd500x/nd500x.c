#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../machine/machine_protos.h"
#include "../../cpu/cpu_protos.h"
#include "../../debugger/debugger.h"

int main(int argc, char** argv) {
	int debug = 0;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--debug") == 0) debug = 1;
	}

	Nd500Machine machine;
	nd500_machine_init(&machine, 8 * 1024 * 1024);
	Nd500Cpu cpu;
	nd500_cpu_init(&cpu, &machine);
	nd500_cpu_reset(&cpu);

	if (debug) {
		return nd500_debugger_repl(&machine);
	}

	printf("nd500x running (no UI). Use --debug for REPL.\n");
	return 0;
}


