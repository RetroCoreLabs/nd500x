#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "debugger.h"
#include "../machine/machine_protos.h"
#include "../ndlib/ndlib.h"
#include "../cpu/cpu_protos.h"

static uint32_t parse_u32(const char* s, uint32_t defv) {
	if (!s || !*s) return defv;
	char* end = NULL;
	unsigned long v = 0;
	if (strncasecmp(s, "0x", 2) == 0) {
		v = strtoul(s + 2, &end, 16);
	} else {
		v = strtoul(s, &end, 10);
	}
	return (uint32_t)v;
}

static void cmd_mem(Nd500Machine* m, const char* a1, const char* a2, uint32_t pc_default) {
	uint32_t addr = parse_u32(a1, pc_default);
	uint32_t len  = parse_u32(a2, 100);
	for (uint32_t i = 0; i < len; i += 16) {
		printf("%08X: ", addr + i);
		for (uint32_t j = 0; j < 16 && (i + j) < len; ++j) {
			uint8_t b = nd500_bus_read8(m, addr + i + j);
			printf("%02X ", b);
		}
		printf("\n");
	}
}

static void cmd_dis(Nd500Machine* m, const char* a1, const char* a2, uint32_t pc_default) {
	char buf[512];
	uint32_t addr = parse_u32(a1, pc_default);
	uint32_t len  = parse_u32(a2, 100);
	nd500_dbg_disasm(m, addr, len, buf, sizeof(buf));
	printf("%s\n", buf);
}

int nd500_debugger_repl(Nd500Machine* m) {
	char line[256];
    printf("nd500x debug mode. Commands: m [addr [len]], d [addr [len]], step [n], regs, load <path>, run, stop, dap <port>, q\n");
	while (fprintf(stdout, "> "), fflush(stdout), fgets(line, sizeof(line), stdin)) {
		char* tok = strtok(line, " \t\r\n");
		if (!tok) continue;
		if (strcmp(tok, "q") == 0 || strcmp(tok, "quit") == 0 || strcmp(tok, "exit") == 0) break;
        else if (strcmp(tok, "m") == 0) {
			char* a1 = strtok(NULL, " \t\r\n");
			char* a2 = strtok(NULL, " \t\r\n");
            uint32_t pc = m->cpu ? m->cpu->PC : 0;
            cmd_mem(m, a1, a2, pc);
		} else if (strcmp(tok, "d") == 0 || strcasecmp(tok, "dis") == 0 || strcasecmp(tok, "disasm") == 0) {
			char* a1 = strtok(NULL, " \t\r\n");
			char* a2 = strtok(NULL, " \t\r\n");
            uint32_t pc = m->cpu ? m->cpu->PC : 0;
            cmd_dis(m, a1, a2, pc);
		} else if (strcmp(tok, "step") == 0) {
			char* a1 = strtok(NULL, " \t\r\n");
			uint32_t n = parse_u32(a1, 1);
			for (uint32_t i = 0; i < n; ++i) nd500_dbg_step(m, 1);
			printf("ok\n");
        } else if (strcmp(tok, "regs") == 0) {
            if (!m->cpu) { printf("no cpu linked\n"); continue; }
            Nd500Regs r; memset(&r, 0, sizeof(r));
            nd500_dbg_regs(m->cpu, &r);
            printf("PC=%08X FLAGS=%08X\n", r.PC, r.FLAGS);
            printf("I: %08X %08X %08X %08X\n", r.I[0], r.I[1], r.I[2], r.I[3]);
            printf("A: %08X %08X %08X %08X\n", r.A[0], r.A[1], r.A[2], r.A[3]);
            printf("E: %08X %08X %08X %08X\n", r.E[0], r.E[1], r.E[2], r.E[3]);
            printf("L=%08X B=%08X R=%08X\n", r.L, r.B, r.R);
            printf("TOS=%08X LL=%08X HL=%08X THA=%08X\n", r.TOS, r.LL, r.HL, r.THA);
            printf("OTE1=%08X OTE2=%08X CTE1=%08X CTE2=%08X\n", r.OTE1, r.OTE2, r.CTE1, r.CTE2);
            printf("MTE1=%08X MTE2=%08X TEMM1=%08X TEMM2=%08X\n", r.MTE1, r.MTE2, r.TEMM1, r.TEMM2);
        } else if (strcmp(tok, "load") == 0) {
			char* path = strtok(NULL, " \t\r\n");
			if (!path) { printf("usage: load <path>\n"); continue; }
			uint32_t entry = 0;
            if (ndlib_loadaout_file(m, path, &entry) == 0) {
				printf("loaded, entry=0x%08X\n", entry);
                if (ndlib_symbols_load(path) == 0) {
                    printf("symbols loaded\n");
                }
			} else {
				printf("load failed\n");
			}
		} else if (strcmp(tok, "run") == 0) {
			nd500_dbg_run(m);
			printf("running...\n");
		} else if (strcmp(tok, "stop") == 0) {
			nd500_dbg_stop(m);
			printf("stopped\n");
        } else if (strcmp(tok, "dap") == 0) {
#ifdef WITH_DEBUGGER
            char* p = strtok(NULL, " \t\r\n");
            int port = p ? (int)parse_u32(p, 47285) : 47285;
            if (nd500_dap_start(m, port) == 0) printf("DAP server started on %d\n", port);
            else printf("failed to start DAP server\n");
#else
            printf("DAP not available (libdap missing)\n");
#endif
        } else {
			printf("unknown command\n");
		}
	}
	return 0;
}


