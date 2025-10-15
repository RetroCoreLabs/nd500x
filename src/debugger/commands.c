/*
 * ND-500 Debugger Shared Command Library
 * Provides unified command interface for both native CLI and WASM web console
 */

#include "commands.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"
#include "../ndlib/ndlib.h"
#include "../cpu/cpu_protos.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* Command handler function type */
typedef int (*cmd_handler_fn)(Nd500Machine* m, CmdContext* ctx, char* args);

/* Command table entry */
typedef struct {
	const char* name;
	cmd_handler_fn handler;
	const char* help;
} CmdEntry;

/* Forward declarations of command handlers */
static int cmd_help(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_mem(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_dis(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_show(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_step(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_regs(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_set(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_load(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_run(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_stop(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_continue(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_symb(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_segments(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_goto(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_msym(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_dsym(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_profile(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_backtrace(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_bp(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_wp(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_clear_traps(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_quit(Nd500Machine* m, CmdContext* ctx, char* args);

/* Command table */
static const CmdEntry g_commands[] = {
	{"help",        cmd_help,         "Show help message"},
	{"?",           cmd_help,         "Show help message"},
	{"m",           cmd_mem,          "Display memory hex dump"},
	{"d",           cmd_dis,          "Disassemble instructions"},
	{"dis",         cmd_dis,          "Disassemble instructions"},
	{"disasm",      cmd_dis,          "Disassemble instructions"},
	{"show",        cmd_show,         "Show/toggle debugger options"},
	{"step",        cmd_step,         "Execute one or more instructions"},
	{"s",           cmd_step,         "Execute one or more instructions"},
	{"regs",        cmd_regs,         "Display CPU registers"},
	{"set",         cmd_set,          "Set register value"},
	{"load",        cmd_load,         "Load binary file"},
	{"run",         cmd_run,          "Start execution"},
	{"stop",        cmd_stop,         "Stop execution"},
	{"continue",    cmd_continue,     "Continue execution"},
	{"c",           cmd_continue,     "Continue execution"},
	{"cont",        cmd_continue,     "Continue execution"},
	{"symb",        cmd_symb,         "List symbols"},
	{"symbols",     cmd_symb,         "List symbols"},
	{"segments",    cmd_segments,     "Show segment layout"},
	{"seg",         cmd_segments,     "Show segment layout"},
	{"goto",        cmd_goto,         "Set PC to symbol"},
	{"msym",        cmd_msym,         "Memory dump at symbol"},
	{"dsym",        cmd_dsym,         "Disassemble at symbol"},
	{"profile",     cmd_profile,      "Show/reset profiling stats"},
	{"backtrace",   cmd_backtrace,    "Show call stack"},
	{"bt",          cmd_backtrace,    "Show call stack"},
	{"bp",          cmd_bp,           "Manage breakpoints"},
	{"break",       cmd_bp,           "Manage breakpoints"},
	{"breakpoint",  cmd_bp,           "Manage breakpoints"},
	{"wp",          cmd_wp,           "Manage watchpoints"},
	{"watch",       cmd_wp,           "Manage watchpoints"},
	{"watchpoint",  cmd_wp,           "Manage watchpoints"},
	{"clear-traps", cmd_clear_traps,  "Clear pending traps"},
	{"q",           cmd_quit,         "Quit debugger"},
	{"quit",        cmd_quit,         "Quit debugger"},
	{"exit",        cmd_quit,         "Quit debugger"},
};

static const int g_command_count = sizeof(g_commands) / sizeof(g_commands[0]);

/* Subcommand lists for autocomplete */
static const char* g_show_subcommands[] = {
	"ea", "demangle", "trace", "profile", "trap", "traps", "trap-status", NULL
};

static const char* g_bp_subcommands[] = {
	"cond", "list", "del", "enable", "disable", NULL
};

static const char* g_wp_subcommands[] = {
	"reg", "list", "del", "enable", "disable", NULL
};

static const char* g_profile_subcommands[] = {
	"show", "reset", NULL
};

static const char* g_set_subcommands[] = {
	"PC", "I1", "I2", "I3", "I4", "A1", "A2", "A3", "A4", "E1", "E2", "E3", "E4",
	"L", "B", "R", "FLAGS", "TOS", "LL", "HL", "THA", "ST1", "ST2",
	"PSTP", "DITBASE", "CED", "CAD", "PS", NULL
};

/* Helper: output a line via callback */
static void output(CmdContext* ctx, const char* fmt, ...) {
	if (!ctx || !ctx->output) return;
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	ctx->output(buf, ctx->context);
}

/* Helper: output an error via callback */
static void error(CmdContext* ctx, const char* fmt, ...) {
	if (!ctx) return;
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	if (ctx->error) {
		ctx->error(buf, ctx->context);
	} else if (ctx->output) {
		ctx->output(buf, ctx->context);
	}
}

/* Parse uint32 from string (hex with 0x or decimal) */
uint32_t nd500_cmd_parse_u32(const char* s, uint32_t defv) {
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

/* Get command list for autocomplete */
const char** nd500_cmd_get_command_list(void) {
	static const char* cmd_list[100];
	static int initialized = 0;

	if (!initialized) {
		int idx = 0;
		for (int i = 0; i < g_command_count && idx < 99; i++) {
			/* Add unique command names only */
			int found = 0;
			for (int j = 0; j < idx; j++) {
				if (strcmp(cmd_list[j], g_commands[i].name) == 0) {
					found = 1;
					break;
				}
			}
			if (!found) {
				cmd_list[idx++] = g_commands[i].name;
			}
		}
		cmd_list[idx] = NULL;
		initialized = 1;
	}

	return cmd_list;
}

/* Get subcommands for a command */
const char** nd500_cmd_get_subcommands(const char* command) {
	if (!command) return NULL;

	if (strcmp(command, "show") == 0) {
		return g_show_subcommands;
	} else if (strcmp(command, "bp") == 0 || strcmp(command, "break") == 0 || strcmp(command, "breakpoint") == 0) {
		return g_bp_subcommands;
	} else if (strcmp(command, "wp") == 0 || strcmp(command, "watch") == 0 || strcmp(command, "watchpoint") == 0) {
		return g_wp_subcommands;
	} else if (strcmp(command, "profile") == 0) {
		return g_profile_subcommands;
	} else if (strcmp(command, "set") == 0) {
		return g_set_subcommands;
	}

	return NULL;
}

/* Execute a command */
int nd500_cmd_execute(Nd500Machine* m, const char* cmdline, CmdContext* ctx) {
	if (!cmdline || !ctx) return -1;

	/* Make a mutable copy of the command line */
	char line[256];
	strncpy(line, cmdline, sizeof(line) - 1);
	line[sizeof(line) - 1] = '\0';

	/* Parse command name */
	char* tok = strtok(line, " \t\r\n");
	if (!tok) return 0;

	/* Find command in table */
	for (int i = 0; i < g_command_count; i++) {
		if (strcmp(tok, g_commands[i].name) == 0) {
			/* Get remaining arguments */
			char* args = strtok(NULL, "");
			return g_commands[i].handler(m, ctx, args);
		}
	}

	error(ctx, "unknown command: %s", tok);
	return -1;
}

/* ═══════════════════════════════════════════════════════ */
/* COMMAND HANDLERS */
/* ═══════════════════════════════════════════════════════ */

static int cmd_help(Nd500Machine* m, CmdContext* ctx, char* args) {
	output(ctx, "Commands:");
	output(ctx, "  help                        Show this help");
	output(ctx, "  m [addr [len]]              Hex dump memory (default addr=PC, len=100)");
	output(ctx, "  d [addr [len]]              Disassemble bytes (default addr=PC, len=100)");
	output(ctx, "  show ea [on|off]            Toggle/show effective-address breakdown in disassembly");
	output(ctx, "  show demangle [on|off]      Toggle C-symbol demangling (strip leading _)");
	output(ctx, "  show trace [on|off]         Toggle instruction execution tracing");
	output(ctx, "  show profile [on|off]      Toggle instruction execution profiling");
	output(ctx, "  profile [show|reset]       Show profiling statistics or reset data");
	output(ctx, "  backtrace (bt)             Show call stack backtrace");
	output(ctx, "  step [n] (s [n])            Execute n instructions (default 1)");
	output(ctx, "  regs                        Show CPU registers");
	output(ctx, "  set <register> <value>      Set register value");
	output(ctx, "  load <path>                 Load ND-500 a.out into memory");
	output(ctx, "  load pseg <path> [mode] [addr]  Load PSEG binary (auto-detect mode from filename)");
	output(ctx, "  load dseg <path> [mode] [addr]  Load DSEG binary (auto-detect mode from filename)");
	output(ctx, "                              mode: kernel (0x08000000) | user (0xD0000000)");
	output(ctx, "  run                         Start execution (background)");
	output(ctx, "  stop                        Stop execution");
	output(ctx, "  continue (c/cont)           Continue execution after breakpoint");
	output(ctx, "  symb (symbols) [type]       List symbols (type: all|text|data|bss)");
	output(ctx, "  segments (seg)              Show TEXT/DATA/BSS segment layout");
	output(ctx, "  goto <symbol>               Set PC to symbol address");
	output(ctx, "  msym <symbol> [len]         Memory dump at symbol address");
	output(ctx, "  dsym <symbol> [len]         Disassemble at symbol address");
	output(ctx, "");
	output(ctx, "Breakpoints:");
	output(ctx, "  bp [addr] (break/breakpoint) Set breakpoint at address (default: PC)");
	output(ctx, "  bp cond <addr> <condition>   Set conditional breakpoint");
	output(ctx, "  bp list                     List all breakpoints");
	output(ctx, "  bp del <id>                 Delete breakpoint");
	output(ctx, "  bp enable <id>              Enable breakpoint");
	output(ctx, "  bp disable <id>             Disable breakpoint");
	output(ctx, "");
	output(ctx, "Watchpoints:");
	output(ctx, "  wp <addr> [len] [type]      Set watchpoint (type: read, write, change)");
	output(ctx, "  wp reg <register>          Set register watchpoint (PC, I1-I4, L, B, R)");
	output(ctx, "  wp list                     List all watchpoints");
	output(ctx, "  wp del <id>                 Delete watchpoint");
	output(ctx, "  wp enable <id>              Enable watchpoint");
	output(ctx, "  wp disable <id>             Disable watchpoint");
	output(ctx, "  (watch/watchpoint)          Alternative names for wp");
	output(ctx, "");
	output(ctx, "Trap System:");
	output(ctx, "  show trap [on|off]          Toggle invalid instruction 0x00 trap");
	output(ctx, "  show traps [on|off]         Show trap system status");
	output(ctx, "  show trap-status            Show current trap status");
	output(ctx, "  clear-traps                 Clear any pending traps");
	output(ctx, "");
	output(ctx, "  q (quit/exit)               Quit debugger");
	return 0;
}

static int cmd_mem(Nd500Machine* m, CmdContext* ctx, char* args) {
	uint32_t pc = m && m->cpu ? m->cpu->PC : 0;
	char* a1 = args ? strtok(args, " \t\r\n") : NULL;
	char* a2 = a1 ? strtok(NULL, " \t\r\n") : NULL;

	uint32_t addr = nd500_cmd_parse_u32(a1, pc);
	uint32_t len = nd500_cmd_parse_u32(a2, 100);

	char line[256];
	for (uint32_t i = 0; i < len; i += 16) {
		uint32_t line_addr = addr + i;
		char hex_part[64];
		char ascii_part[20];
		int hex_pos = 0;

		/* Build hex and ASCII parts */
		for (uint32_t j = 0; j < 16; ++j) {
			uint32_t idx = i + j;
			if (idx < len) {
				uint8_t b = nd500_bus_read8(m, line_addr + j);
				hex_pos += snprintf(hex_part + hex_pos, sizeof(hex_part) - hex_pos, "%02X ", b);
				ascii_part[j] = isprint(b) ? (char)b : '.';
				if (j == 7) hex_pos += snprintf(hex_part + hex_pos, sizeof(hex_part) - hex_pos, " ");
			} else {
				hex_pos += snprintf(hex_part + hex_pos, sizeof(hex_part) - hex_pos, "   ");
				ascii_part[j] = ' ';
			}
		}
		ascii_part[16] = '\0';

		snprintf(line, sizeof(line), "%08X: %-50s |%s|", line_addr, hex_part, ascii_part);
		output(ctx, "%s", line);
	}
	return 0;
}

static int cmd_dis(Nd500Machine* m, CmdContext* ctx, char* args) {
	uint32_t pc = m && m->cpu ? m->cpu->PC : 0;
	char* a1 = args ? strtok(args, " \t\r\n") : NULL;
	char* a2 = a1 ? strtok(NULL, " \t\r\n") : NULL;

	uint32_t addr = nd500_cmd_parse_u32(a1, pc);
	uint32_t len = nd500_cmd_parse_u32(a2, 100);

	/* TODO: Implement callback-based disassembly
	 * For now, use direct printing (will refactor nd500_dbg_disasm_print to use callbacks) */
	nd500_dbg_disasm_print(m, addr, len);
	return 0;
}

static int cmd_show(Nd500Machine* m, CmdContext* ctx, char* args) {
	char* sub = args ? strtok(args, " \t\r\n") : NULL;
	if (!sub) {
		error(ctx, "usage: show ea [on|off]");
		return -1;
	}

	if (strcmp(sub, "ea") == 0) {
		char* val = strtok(NULL, " \t\r\n");
		int newv;
		if (!val) {
			int cur = nd500_dbg_get_show_ea();
			newv = !cur;
		} else if (strcasecmp(val, "on") == 0) {
			newv = 1;
		} else if (strcasecmp(val, "off") == 0) {
			newv = 0;
		} else {
			error(ctx, "usage: show ea [on|off]");
			return -1;
		}
		nd500_dbg_set_show_ea(newv);
		output(ctx, "show ea: %s", newv ? "on" : "off");
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
		} else {
			error(ctx, "usage: show demangle [on|off]");
			return -1;
		}
		nd500_dbg_set_demangle(newv);
		output(ctx, "show demangle: %s", newv ? "on" : "off");
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
		} else {
			error(ctx, "usage: show trace [on|off]");
			return -1;
		}
		nd500_dbg_set_trace_mode(newv);
		output(ctx, "show trace: %s", newv ? "on" : "off");
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
		} else {
			error(ctx, "usage: show profile [on|off]");
			return -1;
		}
		nd500_dbg_set_profiling(newv);
		output(ctx, "show profile: %s", newv ? "on" : "off");
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
			error(ctx, "usage: show trap [on|off]");
			return -1;
		}
		nd500_dbg_set_trap_invalid(newv);
		output(ctx, "show trap: %s", newv ? "on" : "off");
	} else if (strcmp(sub, "traps") == 0) {
		char* val = strtok(NULL, " \t\r\n");
		if (!val) {
			error(ctx, "usage: show traps [on|off]");
			return -1;
		}
		if (strcasecmp(val, "on") == 0) {
			output(ctx, "Trap system: enabled");
			output(ctx, "  - Invalid instruction 0x00 trap: enabled");
			output(ctx, "  - Illegal instruction trap: enabled");
			output(ctx, "  - Trap handler integration: enabled");
		} else if (strcasecmp(val, "off") == 0) {
			output(ctx, "Trap system: disabled");
		} else {
			error(ctx, "usage: show traps [on|off]");
			return -1;
		}
	} else if (strcmp(sub, "trap-status") == 0) {
		if (nd500_dbg_trap_occurred()) {
			const char* desc = nd500_dbg_get_trap_description();
			output(ctx, "Trap occurred: %s", desc ? desc : "Unknown trap");
		} else {
			output(ctx, "No traps pending");
		}
	} else {
		error(ctx, "unknown show option");
		return -1;
	}
	return 0;
}

static int cmd_step(Nd500Machine* m, CmdContext* ctx, char* args) {
	char* a1 = args ? strtok(args, " \t\r\n") : NULL;
	uint32_t n = nd500_cmd_parse_u32(a1, 1);
	for (uint32_t i = 0; i < n; ++i) {
		nd500_dbg_step(m, 1);
	}
	output(ctx, "ok");
	return 0;
}

static int cmd_regs(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}
	Nd500Regs r;
	memset(&r, 0, sizeof(r));
	nd500_dbg_regs(m->cpu, &r);

	output(ctx, "PC=%08X FLAGS=%08X", r.PC, r.FLAGS);
	output(ctx, "I: %08X %08X %08X %08X", r.I[0], r.I[1], r.I[2], r.I[3]);
	output(ctx, "A: %08X %08X %08X %08X", r.A[0], r.A[1], r.A[2], r.A[3]);
	output(ctx, "E: %08X %08X %08X %08X", r.E[0], r.E[1], r.E[2], r.E[3]);
	output(ctx, "L=%08X B=%08X R=%08X", r.L, r.B, r.R);
	output(ctx, "TOS=%08X LL=%08X HL=%08X THA=%08X", r.TOS, r.LL, r.HL, r.THA);
	output(ctx, "OTE1=%08X OTE2=%08X CTE1=%08X CTE2=%08X", r.OTE1, r.OTE2, r.CTE1, r.CTE2);
	output(ctx, "MTE1=%08X MTE2=%08X TEMM1=%08X TEMM2=%08X", r.MTE1, r.MTE2, r.TEMM1, r.TEMM2);
	output(ctx, "PSTP=%08X DITBASE=%08X PS=%08X", r.PSTP, r.DITBASE, r.PS);
	output(ctx, "CED=%08X CAD=%08X", r.CED, r.CAD);
	return 0;
}

static int cmd_set(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	char* reg_name = args ? strtok(args, " \t\r\n") : NULL;
	char* value_str = reg_name ? strtok(NULL, " \t\r\n") : NULL;

	if (!reg_name || !value_str) {
		error(ctx, "usage: set <register> <value>");
		error(ctx, "registers: PC, I1-I4, A1-A4, E1-E4, L, B, R, FLAGS, TOS, LL, HL, THA, ST1, ST2");
		error(ctx, "           PSTP, DITBASE, CED, CAD, PS");
		return -1;
	}

	uint32_t value = nd500_cmd_parse_u32(value_str, 0);

	/* Set register based on name */
	if (strcmp(reg_name, "PC") == 0) {
		m->cpu->PC = value;
		output(ctx, "PC = 0x%08X", value);
	} else if (strcmp(reg_name, "I1") == 0) {
		m->cpu->I[0] = value;
		output(ctx, "I1 = 0x%08X", value);
	} else if (strcmp(reg_name, "I2") == 0) {
		m->cpu->I[1] = value;
		output(ctx, "I2 = 0x%08X", value);
	} else if (strcmp(reg_name, "I3") == 0) {
		m->cpu->I[2] = value;
		output(ctx, "I3 = 0x%08X", value);
	} else if (strcmp(reg_name, "I4") == 0) {
		m->cpu->I[3] = value;
		output(ctx, "I4 = 0x%08X", value);
	} else if (strcmp(reg_name, "A1") == 0) {
		m->cpu->A[0] = value;
		output(ctx, "A1 = 0x%08X", value);
	} else if (strcmp(reg_name, "A2") == 0) {
		m->cpu->A[1] = value;
		output(ctx, "A2 = 0x%08X", value);
	} else if (strcmp(reg_name, "A3") == 0) {
		m->cpu->A[2] = value;
		output(ctx, "A3 = 0x%08X", value);
	} else if (strcmp(reg_name, "A4") == 0) {
		m->cpu->A[3] = value;
		output(ctx, "A4 = 0x%08X", value);
	} else if (strcmp(reg_name, "E1") == 0) {
		m->cpu->E[0] = value;
		output(ctx, "E1 = 0x%08X", value);
	} else if (strcmp(reg_name, "E2") == 0) {
		m->cpu->E[1] = value;
		output(ctx, "E2 = 0x%08X", value);
	} else if (strcmp(reg_name, "E3") == 0) {
		m->cpu->E[2] = value;
		output(ctx, "E3 = 0x%08X", value);
	} else if (strcmp(reg_name, "E4") == 0) {
		m->cpu->E[3] = value;
		output(ctx, "E4 = 0x%08X", value);
	} else if (strcmp(reg_name, "L") == 0) {
		m->cpu->L = value;
		output(ctx, "L = 0x%08X", value);
	} else if (strcmp(reg_name, "B") == 0) {
		m->cpu->B = value;
		output(ctx, "B = 0x%08X", value);
	} else if (strcmp(reg_name, "R") == 0) {
		m->cpu->R = value;
		output(ctx, "R = 0x%08X", value);
	} else if (strcmp(reg_name, "FLAGS") == 0) {
		m->cpu->FLAGS = value;
		output(ctx, "FLAGS = 0x%08X", value);
	} else if (strcmp(reg_name, "TOS") == 0) {
		m->cpu->TOS = value;
		output(ctx, "TOS = 0x%08X", value);
	} else if (strcmp(reg_name, "LL") == 0) {
		m->cpu->LL = value;
		output(ctx, "LL = 0x%08X", value);
	} else if (strcmp(reg_name, "HL") == 0) {
		m->cpu->HL = value;
		output(ctx, "HL = 0x%08X", value);
	} else if (strcmp(reg_name, "THA") == 0) {
		m->cpu->THA = value;
		output(ctx, "THA = 0x%08X", value);
	} else if (strcmp(reg_name, "ST1") == 0) {
		m->cpu->ST1 = value;
		output(ctx, "ST1 = 0x%08X", value);
	} else if (strcmp(reg_name, "ST2") == 0) {
		m->cpu->ST2 = value;
		output(ctx, "ST2 = 0x%08X", value);
	} else if (strcmp(reg_name, "PSTP") == 0) {
		m->cpu->PSTP = value;
		output(ctx, "PSTP = 0x%08X", value);
	} else if (strcmp(reg_name, "DITBASE") == 0) {
		m->cpu->DITBASE = value;
		output(ctx, "DITBASE = 0x%08X", value);
	} else if (strcmp(reg_name, "CED") == 0) {
		m->cpu->CED = value;
		output(ctx, "CED = 0x%08X", value);
	} else if (strcmp(reg_name, "CAD") == 0) {
		m->cpu->CAD = value;
		output(ctx, "CAD = 0x%08X", value);
	} else if (strcmp(reg_name, "PS") == 0) {
		m->cpu->PS = value;
		output(ctx, "PS = 0x%08X", value);
	} else {
		error(ctx, "unknown register: %s", reg_name);
		error(ctx, "registers: PC, I1-I4, A1-A4, E1-E4, L, B, R, FLAGS, TOS, LL, HL, THA, ST1, ST2");
		error(ctx, "           PSTP, DITBASE, CED, CAD, PS");
		return -1;
	}
	return 0;
}

static int cmd_load(Nd500Machine* m, CmdContext* ctx, char* args) {
	/* This command is complex and involves file I/O
	 * For now, output an error that this needs native file access */
	error(ctx, "load command not yet supported in shared library (requires file I/O refactoring)");
	return -1;
}

static int cmd_run(Nd500Machine* m, CmdContext* ctx, char* args) {
	nd500_dbg_run(m);
	output(ctx, "running...");
	return 0;
}

static int cmd_stop(Nd500Machine* m, CmdContext* ctx, char* args) {
	nd500_dbg_stop(m);
	output(ctx, "stopped");
	return 0;
}

static int cmd_continue(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}
	nd500_dbg_clear_traps();
	nd500_dbg_run(m);
	output(ctx, "continuing...");
	return 0;
}

static int cmd_symb(Nd500Machine* m, CmdContext* ctx, char* args) {
	/* TODO: Implement callback-based symbol listing */
	ndlib_symbols_list_all();
	return 0;
}

static int cmd_segments(Nd500Machine* m, CmdContext* ctx, char* args) {
	uint32_t text_base, text_size, data_base, data_size, bss_base, bss_size;
	ndlib_aout_get_segment_info(&text_base, &text_size, &data_base, &data_size, &bss_base, &bss_size);

	output(ctx, "=== SEGMENT LAYOUT ===");
	output(ctx, "TEXT: 0x%08X - 0x%08X (%u bytes)", text_base, text_base + text_size, text_size);
	output(ctx, "DATA: 0x%08X - 0x%08X (%u bytes)", data_base, data_base + data_size, data_size);
	output(ctx, "BSS:  0x%08X - 0x%08X (%u bytes)", bss_base, bss_base + bss_size, bss_size);
	output(ctx, "Total: %u bytes", text_size + data_size + bss_size);
	return 0;
}

static int cmd_goto(Nd500Machine* m, CmdContext* ctx, char* args) {
	char* symbol_name = args ? strtok(args, " \t\r\n") : NULL;
	if (!symbol_name) {
		error(ctx, "usage: goto <symbol>");
		return -1;
	}

	if (!m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	uint32_t addr;
	if (ndlib_symbols_absolute_addr(symbol_name, &addr) == 0) {
		m->cpu->PC = addr;
		output(ctx, "PC = 0x%08X (%s)", addr, symbol_name);
	} else {
		error(ctx, "symbol not found: %s", symbol_name);
		return -1;
	}
	return 0;
}

static int cmd_msym(Nd500Machine* m, CmdContext* ctx, char* args) {
	char* symbol_name = args ? strtok(args, " \t\r\n") : NULL;
	char* len_str = symbol_name ? strtok(NULL, " \t\r\n") : NULL;

	if (!symbol_name) {
		error(ctx, "usage: msym <symbol> [length]");
		return -1;
	}

	uint32_t addr;
	uint8_t type;
	if (ndlib_symbols_lookup(symbol_name, &addr, &type) != 0) {
		error(ctx, "symbol not found: %s", symbol_name);
		return -1;
	}

	uint32_t absolute_addr;
	if (ndlib_symbols_absolute_addr(symbol_name, &absolute_addr) != 0) {
		error(ctx, "cannot calculate absolute address for: %s", symbol_name);
		return -1;
	}

	uint32_t len = nd500_cmd_parse_u32(len_str, 100);
	output(ctx, "Memory dump at %s (0x%08X):", symbol_name, absolute_addr);

	/* Reuse cmd_mem logic */
	char addr_buf[32];
	snprintf(addr_buf, sizeof(addr_buf), "0x%X", absolute_addr);
	char len_buf[32];
	snprintf(len_buf, sizeof(len_buf), "%u", len);
	char combined_args[128];
	snprintf(combined_args, sizeof(combined_args), "%s %s", addr_buf, len_buf);
	return cmd_mem(m, ctx, combined_args);
}

static int cmd_dsym(Nd500Machine* m, CmdContext* ctx, char* args) {
	char* symbol_name = args ? strtok(args, " \t\r\n") : NULL;
	char* len_str = symbol_name ? strtok(NULL, " \t\r\n") : NULL;

	if (!symbol_name) {
		error(ctx, "usage: dsym <symbol> [length]");
		return -1;
	}

	uint32_t addr;
	uint8_t type;
	if (ndlib_symbols_lookup(symbol_name, &addr, &type) != 0) {
		error(ctx, "symbol not found: %s", symbol_name);
		return -1;
	}

	if ((type & 0x0E) != 0x04) {
		output(ctx, "warning: %s is not a TEXT symbol", symbol_name);
	}

	uint32_t absolute_addr;
	if (ndlib_symbols_absolute_addr(symbol_name, &absolute_addr) != 0) {
		error(ctx, "cannot calculate absolute address for: %s", symbol_name);
		return -1;
	}

	uint32_t len = nd500_cmd_parse_u32(len_str, 100);
	output(ctx, "Disassembly at %s (0x%08X):", symbol_name, absolute_addr);

	/* Reuse cmd_dis logic */
	char addr_buf[32];
	snprintf(addr_buf, sizeof(addr_buf), "0x%X", absolute_addr);
	char len_buf[32];
	snprintf(len_buf, sizeof(len_buf), "%u", len);
	char combined_args[128];
	snprintf(combined_args, sizeof(combined_args), "%s %s", addr_buf, len_buf);
	return cmd_dis(m, ctx, combined_args);
}

static int cmd_profile(Nd500Machine* m, CmdContext* ctx, char* args) {
	char* subcmd = args ? strtok(args, " \t\r\n") : NULL;
	if (!subcmd || strcmp(subcmd, "show") == 0) {
		nd500_dbg_show_profile();
	} else if (strcmp(subcmd, "reset") == 0) {
		nd500_dbg_reset_profile();
	} else {
		error(ctx, "usage: profile [show|reset]");
		return -1;
	}
	return 0;
}

static int cmd_backtrace(Nd500Machine* m, CmdContext* ctx, char* args) {
	nd500_dbg_show_backtrace();
	return 0;
}

static int cmd_bp(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m->bp_mgr) {
		error(ctx, "no breakpoint manager");
		return -1;
	}

	char* a1 = args ? strtok(args, " \t\r\n") : NULL;
	char* a2 = a1 ? strtok(NULL, " \t\r\n") : NULL;

	if (!a1 || strcasecmp(a1, "list") == 0 || strcasecmp(a1, "ls") == 0) {
		bp_list(m->bp_mgr);
	} else if (strcasecmp(a1, "del") == 0 || strcasecmp(a1, "delete") == 0) {
		if (!a2) {
			error(ctx, "usage: bp del <id>");
			return -1;
		}
		int id = (int)nd500_cmd_parse_u32(a2, -1);
		bp_delete(m->bp_mgr, id);
	} else if (strcasecmp(a1, "enable") == 0 || strcasecmp(a1, "en") == 0) {
		if (!a2) {
			error(ctx, "usage: bp enable <id>");
			return -1;
		}
		int id = (int)nd500_cmd_parse_u32(a2, -1);
		bp_enable(m->bp_mgr, id);
	} else if (strcasecmp(a1, "disable") == 0 || strcasecmp(a1, "dis") == 0) {
		if (!a2) {
			error(ctx, "usage: bp disable <id>");
			return -1;
		}
		int id = (int)nd500_cmd_parse_u32(a2, -1);
		bp_disable(m->bp_mgr, id);
	} else if (strcasecmp(a1, "cond") == 0 || strcasecmp(a1, "conditional") == 0) {
		if (!a2) {
			error(ctx, "usage: bp cond <addr> <condition>");
			return -1;
		}
		uint32_t addr = nd500_cmd_parse_u32(a2, m->cpu ? m->cpu->PC : 0);
		char* condition = strtok(NULL, "\r\n");
		if (!condition) {
			error(ctx, "usage: bp cond <addr> <condition>");
			return -1;
		}
		bp_add_conditional(m->bp_mgr, addr, condition, false);
	} else {
		uint32_t addr = nd500_cmd_parse_u32(a1, m->cpu ? m->cpu->PC : 0);
		bp_add(m->bp_mgr, addr, false);
	}
	return 0;
}

static int cmd_wp(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m->bp_mgr) {
		error(ctx, "no breakpoint manager");
		return -1;
	}

	char* a1 = args ? strtok(args, " \t\r\n") : NULL;
	char* a2 = a1 ? strtok(NULL, " \t\r\n") : NULL;
	char* a3 = a2 ? strtok(NULL, " \t\r\n") : NULL;

	if (!a1 || strcasecmp(a1, "list") == 0 || strcasecmp(a1, "ls") == 0) {
		wp_list(m->bp_mgr);
	} else if (strcasecmp(a1, "del") == 0 || strcasecmp(a1, "delete") == 0) {
		if (!a2) {
			error(ctx, "usage: wp del <id>");
			return -1;
		}
		int id = (int)nd500_cmd_parse_u32(a2, -1);
		wp_delete(m->bp_mgr, id);
	} else if (strcasecmp(a1, "enable") == 0 || strcasecmp(a1, "en") == 0) {
		if (!a2) {
			error(ctx, "usage: wp enable <id>");
			return -1;
		}
		int id = (int)nd500_cmd_parse_u32(a2, -1);
		wp_enable(m->bp_mgr, id);
	} else if (strcasecmp(a1, "disable") == 0 || strcasecmp(a1, "dis") == 0) {
		if (!a2) {
			error(ctx, "usage: wp disable <id>");
			return -1;
		}
		int id = (int)nd500_cmd_parse_u32(a2, -1);
		wp_disable(m->bp_mgr, id);
	} else if (strcasecmp(a1, "reg") == 0 || strcasecmp(a1, "register") == 0) {
		if (!a2) {
			error(ctx, "usage: wp reg <register_name>");
			return -1;
		}
		uint32_t reg_index = 0;
		if (strcasecmp(a2, "PC") == 0) reg_index = 0;
		else if (strcasecmp(a2, "I1") == 0) reg_index = 1;
		else if (strcasecmp(a2, "I2") == 0) reg_index = 2;
		else if (strcasecmp(a2, "I3") == 0) reg_index = 3;
		else if (strcasecmp(a2, "I4") == 0) reg_index = 4;
		else if (strcasecmp(a2, "L") == 0) reg_index = 5;
		else if (strcasecmp(a2, "B") == 0) reg_index = 6;
		else if (strcasecmp(a2, "R") == 0) reg_index = 7;
		else {
			error(ctx, "Unknown register: %s", a2);
			return -1;
		}
		wp_add_register(m->bp_mgr, a2, reg_index);
	} else {
		uint32_t addr = nd500_cmd_parse_u32(a1, 0);
		uint32_t len = a2 ? nd500_cmd_parse_u32(a2, 4) : 4;
		WatchpointType type = WP_TYPE_WRITE;

		if (a3) {
			if (strcasecmp(a3, "read") == 0 || strcasecmp(a3, "r") == 0) type = WP_TYPE_READ;
			else if (strcasecmp(a3, "write") == 0 || strcasecmp(a3, "w") == 0) type = WP_TYPE_WRITE;
			else if (strcasecmp(a3, "change") == 0 || strcasecmp(a3, "c") == 0) type = WP_TYPE_CHANGE;
		}

		wp_add(m->bp_mgr, addr, len, type);
	}
	return 0;
}

static int cmd_clear_traps(Nd500Machine* m, CmdContext* ctx, char* args) {
	nd500_dbg_clear_traps();
	output(ctx, "Traps cleared");
	return 0;
}

static int cmd_quit(Nd500Machine* m, CmdContext* ctx, char* args) {
	output(ctx, "quitting...");
	return 1; /* Return 1 to signal quit */
}
