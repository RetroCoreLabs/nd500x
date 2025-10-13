#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "debugger.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"
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
        uint32_t line_addr = addr + i;
        char ascii[17];
        ascii[16] = '\0';
        /* ANSI colors */
        const char *c_reset = "\x1b[0m";
        const char *c_addr  = "\x1b[36m";   /* cyan */
        const char *c_dim   = "\x1b[90m";   /* bright black (dim) */

        /* Address */
        printf("%s%08X%s: ", c_addr, line_addr, c_reset);

        /* Hex bytes (grouped 8+8) */
        for (uint32_t j = 0; j < 16; ++j) {
            uint32_t idx = i + j;
            if (idx < len) {
                uint8_t b = nd500_bus_read8(m, line_addr + j);
                /* Dim zero bytes to make patterns pop */
                if (b == 0x00) printf("%s%02X%s ", c_dim, b, c_reset);
                else           printf("%02X ", b);
                ascii[j] = isprint(b) ? (char)b : '.';
            } else {
                printf("   ");
                ascii[j] = ' ';
            }
            if (j == 7) printf(" "); /* extra gap between 8-byte groups */
        }

        /* ASCII column */
        printf(" |");
        for (uint32_t j = 0; j < 16; ++j) {
            char ch = ascii[j];
            if (ch == '\0') ch = ' ';
            if (ch == '.') {
                printf("%s.%s", c_dim, c_reset);
            } else {
                putchar(ch);
            }
        }
        printf("|\n");
    }
}

static void cmd_dis(Nd500Machine* m, const char* a1, const char* a2, uint32_t pc_default) {
	uint32_t addr = parse_u32(a1, pc_default);
	uint32_t len  = parse_u32(a2, 100);
	nd500_dbg_disasm_print(m, addr, len);
}

int nd500_debugger_repl(Nd500Machine* m) {
	char line[256];
    printf("nd500x debug mode. Commands: m, d, step, regs, load, run, stop, symb, show, bp, wp, continue, help, q\n");
    while (
        /* Colorized prompt: cyan PC inside dim brackets */
        fprintf(stdout, "\x1b[90m[\x1b[0m\x1b[36m%08X\x1b[0m\x1b[90m]\x1b[0m ",
                m && m->cpu ? m->cpu->PC : 0),
        fflush(stdout),
        fgets(line, sizeof(line), stdin)) {
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
        } else if (strcmp(tok, "show") == 0) {
            char* sub = strtok(NULL, " \t\r\n");
            if (!sub) { printf("usage: show ea [on|off]\n"); continue; }
            if (strcmp(sub, "ea") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                int newv;
                if (!val) {
                    /* toggle */
                    int cur = nd500_dbg_get_show_ea();
                    newv = !cur;
                } else if (strcasecmp(val, "on") == 0) {
                    newv = 1;
                } else if (strcasecmp(val, "off") == 0) {
                    newv = 0;
                } else {
                    printf("usage: show ea [on|off]\n");
                    continue;
                }
                nd500_dbg_set_show_ea(newv);
                printf("show ea: %s\n", newv ? "on" : "off");
            } else if (strcmp(sub, "demangle") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                int newv;
                if (!val) {
                    int cur = nd500_dbg_get_demangle();
                    newv = !cur;
                } else if (strcasecmp(val, "on") == 0) {
                    newv = 1;
                } else if (strcasecmp(val, "off") == 0) {
                    newv = 0;
                } else { printf("usage: show demangle [on|off]\n"); continue; }
                nd500_dbg_set_demangle(newv);
                printf("show demangle: %s\n", newv ? "on" : "off");
            } else if (strcmp(sub, "trace") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                int newv;
                if (!val) {
                    int cur = nd500_dbg_get_trace_mode();
                    newv = !cur;
                } else if (strcasecmp(val, "on") == 0) {
                    newv = 1;
                } else if (strcasecmp(val, "off") == 0) {
                    newv = 0;
                } else { printf("usage: show trace [on|off]\n"); continue; }
                nd500_dbg_set_trace_mode(newv);
                printf("show trace: %s\n", newv ? "on" : "off");
            } else if (strcmp(sub, "profile") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                int newv;
                if (!val) {
                    int cur = nd500_dbg_get_profiling();
                    newv = !cur;
                } else if (strcasecmp(val, "on") == 0) {
                    newv = 1;
                } else if (strcasecmp(val, "off") == 0) {
                    newv = 0;
                } else { printf("usage: show profile [on|off]\n"); continue; }
                nd500_dbg_set_profiling(newv);
                printf("show profile: %s\n", newv ? "on" : "off");
            } else if (strcmp(sub, "trap") == 0) {
                char* val = strtok(NULL, " \t\r\n");
                int newv;
                if (!val) {
                    int cur = nd500_dbg_get_trap_invalid();
                    newv = !cur;
                } else if (strcasecmp(val, "on") == 0) {
                    newv = 1;
                } else if (strcasecmp(val, "off") == 0) {
                    newv = 0;
                } else {
                    printf("usage: show trap [on|off]\n");
                    continue;
                }
                nd500_dbg_set_trap_invalid(newv);
                printf("show trap: %s\n", newv ? "on" : "off");
            } else {
                printf("unknown show option\n");
            }
        } else if (strcmp(tok, "step") == 0 || strcmp(tok, "s") == 0) {
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
                /* Print metadata similar to nd500-dump */
                (void)ndlib_aout_dump_metadata(path);
			} else {
				printf("load failed\n");
			}
		} else if (strcmp(tok, "run") == 0) {
			nd500_dbg_run(m);
			printf("running...\n");
		} else if (strcmp(tok, "stop") == 0) {
			nd500_dbg_stop(m);
			printf("stopped\n");
        } else if (strcmp(tok, "symb") == 0 || strcmp(tok, "symbols") == 0) {
			ndlib_symbols_list_all();
        } else if (strcmp(tok, "profile") == 0) {
            char* subcmd = strtok(NULL, " \t\r\n");
            if (!subcmd || strcmp(subcmd, "show") == 0) {
                nd500_dbg_show_profile();
            } else if (strcmp(subcmd, "reset") == 0) {
                nd500_dbg_reset_profile();
            } else {
                printf("usage: profile [show|reset]\n");
            }
        } else if (strcmp(tok, "backtrace") == 0 || strcmp(tok, "bt") == 0) {
            nd500_dbg_show_backtrace();
        } else if (strcmp(tok, "bp") == 0 || strcmp(tok, "break") == 0 || strcmp(tok, "breakpoint") == 0) {
			/* Breakpoint commands: bp [addr], bp list, bp del <id>, bp enable <id>, bp disable <id> */
			char* a1 = strtok(NULL, " \t\r\n");
			char* a2 = strtok(NULL, " \t\r\n");
			
			if (!m->bp_mgr) { printf("no breakpoint manager\n"); continue; }
			
			if (!a1 || strcasecmp(a1, "list") == 0 || strcasecmp(a1, "ls") == 0) {
				bp_list(m->bp_mgr);
			} else if (strcasecmp(a1, "del") == 0 || strcasecmp(a1, "delete") == 0) {
				if (!a2) { printf("usage: bp del <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				bp_delete(m->bp_mgr, id);
			} else if (strcasecmp(a1, "enable") == 0 || strcasecmp(a1, "en") == 0) {
				if (!a2) { printf("usage: bp enable <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				bp_enable(m->bp_mgr, id);
			} else if (strcasecmp(a1, "disable") == 0 || strcasecmp(a1, "dis") == 0) {
				if (!a2) { printf("usage: bp disable <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				bp_disable(m->bp_mgr, id);
			} else if (strcasecmp(a1, "cond") == 0 || strcasecmp(a1, "conditional") == 0) {
				/* Conditional breakpoint: bp cond <addr> <condition> */
				if (!a2) { printf("usage: bp cond <addr> <condition>\n"); continue; }
				uint32_t addr = parse_u32(a2, m->cpu ? m->cpu->PC : 0);
				char* condition = strtok(NULL, "\r\n"); /* Get rest of line as condition */
				if (!condition) { printf("usage: bp cond <addr> <condition>\n"); continue; }
				bp_add_conditional(m->bp_mgr, addr, condition, false);
			} else {
				/* Set breakpoint at address */
				uint32_t addr = parse_u32(a1, m->cpu ? m->cpu->PC : 0);
				bp_add(m->bp_mgr, addr, false);
			}
        } else if (strcmp(tok, "wp") == 0 || strcmp(tok, "watch") == 0 || strcmp(tok, "watchpoint") == 0) {
			/* Watchpoint commands: wp <addr> [len] [type], wp reg <reg>, wp list, wp del <id> */
			char* a1 = strtok(NULL, " \t\r\n");
			char* a2 = strtok(NULL, " \t\r\n");
			char* a3 = strtok(NULL, " \t\r\n");
			
			if (!m->bp_mgr) { printf("no breakpoint manager\n"); continue; }
			
			if (!a1 || strcasecmp(a1, "list") == 0 || strcasecmp(a1, "ls") == 0) {
				wp_list(m->bp_mgr);
			} else if (strcasecmp(a1, "del") == 0 || strcasecmp(a1, "delete") == 0) {
				if (!a2) { printf("usage: wp del <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				wp_delete(m->bp_mgr, id);
			} else if (strcasecmp(a1, "enable") == 0 || strcasecmp(a1, "en") == 0) {
				if (!a2) { printf("usage: wp enable <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				wp_enable(m->bp_mgr, id);
			} else if (strcasecmp(a1, "disable") == 0 || strcasecmp(a1, "dis") == 0) {
				if (!a2) { printf("usage: wp disable <id>\n"); continue; }
				int id = (int)parse_u32(a2, -1);
				wp_disable(m->bp_mgr, id);
			} else if (strcasecmp(a1, "reg") == 0 || strcasecmp(a1, "register") == 0) {
				/* Register watchpoint: wp reg <reg_name> */
				if (!a2) { printf("usage: wp reg <register_name>\n"); continue; }
				
				/* Map register names to indices */
				uint32_t reg_index = 0;
				if (strcasecmp(a2, "PC") == 0) reg_index = 0;
				else if (strcasecmp(a2, "I1") == 0) reg_index = 1;
				else if (strcasecmp(a2, "I2") == 0) reg_index = 2;
				else if (strcasecmp(a2, "I3") == 0) reg_index = 3;
				else if (strcasecmp(a2, "I4") == 0) reg_index = 4;
				else if (strcasecmp(a2, "L") == 0) reg_index = 5;
				else if (strcasecmp(a2, "B") == 0) reg_index = 6;
				else if (strcasecmp(a2, "R") == 0) reg_index = 7;
				else { printf("Unknown register: %s\n", a2); continue; }
				
				wp_add_register(m->bp_mgr, a2, reg_index);
			} else {
				/* Set watchpoint: wp <addr> [len] [read|write|change] */
				uint32_t addr = parse_u32(a1, 0);
				uint32_t len = a2 ? parse_u32(a2, 4) : 4;
				WatchpointType type = WP_TYPE_WRITE; /* default */
				
				if (a3) {
					if (strcasecmp(a3, "read") == 0 || strcasecmp(a3, "r") == 0) type = WP_TYPE_READ;
					else if (strcasecmp(a3, "write") == 0 || strcasecmp(a3, "w") == 0) type = WP_TYPE_WRITE;
					else if (strcasecmp(a3, "change") == 0 || strcasecmp(a3, "c") == 0) type = WP_TYPE_CHANGE;
				}
				
				wp_add(m->bp_mgr, addr, len, type);
			}
        } else if (strcmp(tok, "continue") == 0 || strcmp(tok, "c") == 0 || strcmp(tok, "cont") == 0) {
			/* Continue execution after hitting a breakpoint */
			if (!m->cpu) { printf("no cpu linked\n"); continue; }
			nd500_dbg_run(m);
			printf("continuing...\n");
        } else if (strcmp(tok, "dap") == 0) {
#ifdef WITH_DEBUGGER
            char* p = strtok(NULL, " \t\r\n");
            int port = p ? (int)parse_u32(p, 47285) : 47285;
            if (nd500_dap_start(m, port) == 0) printf("DAP server started on %d\n", port);
            else printf("failed to start DAP server\n");
#else
            printf("DAP not available (libdap missing)\n");
#endif
        } else if (strcmp(tok, "help") == 0 || strcmp(tok, "?") == 0) {
            printf("Commands:\n");
            printf("  help                        Show this help\n");
            printf("  m [addr [len]]              Hex dump memory (default addr=PC, len=100)\n");
            printf("  d [addr [len]]              Disassemble bytes (default addr=PC, len=100)\n");
            printf("  show ea [on|off]            Toggle/show effective-address breakdown in disassembly\n");
            printf("  show demangle [on|off]      Toggle C-symbol demangling (strip leading _)\n");
            printf("  show trace [on|off]         Toggle instruction execution tracing\n");
            printf("  show profile [on|off]      Toggle instruction execution profiling\n");
            printf("  show trap [on|off]          Toggle invalid instruction 0x00 trap\n");
            printf("  profile [show|reset]       Show profiling statistics or reset data\n");
            printf("  backtrace (bt)             Show call stack backtrace\n");
            printf("  step [n] (s [n])            Execute n instructions (default 1)\n");
            printf("  regs                        Show CPU registers\n");
            printf("  load <path>                 Load ND-500 a.out into memory\n");
            printf("  run                         Start execution (background)\n");
            printf("  stop                        Stop execution\n");
            printf("  continue (c/cont)           Continue execution after breakpoint\n");
            printf("  symb (symbols)              List all symbols\n");
            printf("\n");
            printf("Breakpoints:\n");
            printf("  bp [addr] (break/breakpoint) Set breakpoint at address (default: PC)\n");
            printf("  bp cond <addr> <condition>   Set conditional breakpoint\n");
            printf("  bp list                     List all breakpoints\n");
            printf("  bp del <id>                 Delete breakpoint\n");
            printf("  bp enable <id>              Enable breakpoint\n");
            printf("  bp disable <id>             Disable breakpoint\n");
            printf("\n");
            printf("Watchpoints:\n");
            printf("  wp <addr> [len] [type]      Set watchpoint (type: read, write, change)\n");
            printf("  wp reg <register>          Set register watchpoint (PC, I1-I4, L, B, R)\n");
            printf("  wp list                     List all watchpoints\n");
            printf("  wp del <id>                 Delete watchpoint\n");
            printf("  wp enable <id>              Enable watchpoint\n");
            printf("  wp disable <id>             Disable watchpoint\n");
            printf("  (watch/watchpoint)          Alternative names for wp\n");
            printf("\n");
            printf("  dap <port>                  Start DAP server on port (WITH_DEBUGGER)\n");
            printf("  q (quit/exit)               Quit\n");
        } else {
			printf("unknown command\n");
		}
	}
	return 0;
}


