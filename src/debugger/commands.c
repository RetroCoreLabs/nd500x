/*
 * ND-500 Debugger Shared Command Library
 * Provides unified command interface for both native CLI and WASM web console
 */

#include "commands.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"
#include "../ndlib/ndlib.h"
#include "../cpu/cpu_protos.h"
#include "../cpu/nd500_mmu.h"
#include "../cpu/nd500_domain.h"
#include "nd500_dom.h"
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
static int cmd_mem_prog(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_dis(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_show(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_step(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_regs(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_set(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_load(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_load_pseg(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_load_dseg(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_loadmap(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_loadsrc(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_loaddom(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_run(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_stop(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_continue(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_symb(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_segments(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_goto(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_msym(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_dsym(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_list(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_profile(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_backtrace(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_bp(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_wp(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_clear_traps(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_mmu(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_showmmu(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_showpst(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_showpcb(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_phyladr(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_mmusetup(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_listpst(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_listpcb(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_quit(Nd500Machine* m, CmdContext* ctx, char* args);

/* Forward declaration for init script execution (defined at end of file) */
int nd500_execute_init_script(Nd500Machine* m, const char* script_path);

/* Command table */
static const CmdEntry g_commands[] = {
	{"help",        cmd_help,         "Show help message"},
	{"?",           cmd_help,         "Show help message"},
	{"m",           cmd_mem,          "Display memory hex dump (data space)"},
	{"mp",          cmd_mem_prog,     "Display memory hex dump (program space)"},
	{"d",           cmd_dis,          "Disassemble instructions"},
	{"dis",         cmd_dis,          "Disassemble instructions"},
	{"disasm",      cmd_dis,          "Disassemble instructions"},
	{"show",        cmd_show,         "Show/toggle debugger options"},
	{"step",        cmd_step,         "Execute one or more instructions"},
	{"s",           cmd_step,         "Execute one or more instructions"},
	{"regs",        cmd_regs,         "Display CPU registers"},
	{"set",         cmd_set,          "Set register value"},
	{"reg",         cmd_set,          "Set register value (alias for set)"},
	{"load",        cmd_load,         "Load binary file"},
	{"load-pseg",   cmd_load_pseg,    "Load PSEG binary file"},
	{"load-dseg",   cmd_load_dseg,    "Load DSEG binary file"},
	{"loadmap",     cmd_loadmap,      "Load additional map file"},
	{"loadsrc",     cmd_loadsrc,      "Load additional source file"},
	{"loaddom",     cmd_loaddom,      "Load DOM/SEG file header"},
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
	{"list",        cmd_list,         "List source code"},
	{"l",           cmd_list,         "List source code"},
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
	{"mmu",         cmd_mmu,          "Control Program/Data MMU"},
	{"showmmu",     cmd_showmmu,      "Show detailed MMU status"},
	{"showpst",     cmd_showpst,      "Show PST entry"},
	{"showpcb",     cmd_showpcb,      "Show PCB capabilities"},
	{"phyladr",     cmd_phyladr,      "Translate virtual to physical address"},
	{"mmusetup",    cmd_mmusetup,     "Setup demo MMU configuration"},
	{"listpst",     cmd_listpst,      "List configured PST entries"},
	{"listpcb",     cmd_listpcb,      "List configured PCB domains"},
	{"q",           cmd_quit,         "Quit debugger"},
	{"quit",        cmd_quit,         "Quit debugger"},
	{"exit",        cmd_quit,         "Quit debugger"},
};

static const int g_command_count = sizeof(g_commands) / sizeof(g_commands[0]);

/* Subcommand lists for autocomplete */
static const char* g_show_subcommands[] = {
	"ea", "hex", "demangle", "source", "trace", "profile", "trap", "traps", "trap-status", NULL
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
	"PSTP", "DITBASE", "CED", "CAD", "PS", "radix", NULL
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

/* Parse uint32 from string with radix-aware parsing and override prefixes:
 *   $10   = decimal 10 (always)
 *   0x10  = hex 16 (always)
 *   010   = octal 8 (always, leading zero followed by digit)
 *   10    = depends on current radix setting
 */
uint32_t nd500_cmd_parse_u32(const char* s, uint32_t defv) {
	if (!s || !*s) return defv;
	char* end = NULL;
	unsigned long v = 0;

	/* Override prefix: $ = decimal */
	if (s[0] == '$') {
		v = strtoul(s + 1, &end, 10);
		return (uint32_t)v;
	}
	/* Override prefix: 0x = hex */
	if (strncasecmp(s, "0x", 2) == 0) {
		v = strtoul(s + 2, &end, 16);
		return (uint32_t)v;
	}
	/* Override prefix: leading 0 followed by digit = octal */
	if (s[0] == '0' && s[1] >= '0' && s[1] <= '7') {
		v = strtoul(s, &end, 8);
		return (uint32_t)v;
	}
	/* No override prefix: use current radix */
	int base = nd500_dbg_get_radix_base();
	v = strtoul(s, &end, base);
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
	output(ctx, "  show hex [on|off]           Toggle hex bytes in disassembly (default: on)");
	output(ctx, "  show demangle [on|off]      Toggle C-symbol demangling (strip leading _)");
	output(ctx, "  show source [off|asm|c|both] Set source annotations in disassembly");
	output(ctx, "  show trace [on|off]         Toggle instruction execution tracing");
	output(ctx, "  show profile [on|off]      Toggle instruction execution profiling");
	output(ctx, "  profile [show|reset]       Show profiling statistics or reset data");
	output(ctx, "  backtrace (bt)             Show call stack backtrace");
	output(ctx, "  step [n] (s [n])            Execute n instructions (default 1)");
	output(ctx, "  regs                        Show CPU registers");
	output(ctx, "  set <register> <value>      Set register value");
	output(ctx, "  set radix [decimal|hex|octal] Set numeric format for disasm and input");
	output(ctx, "  load <path>                 Load ND-500 a.out into memory");
	output(ctx, "  load-pseg <path> [addr]     Load PSEG binary (addr: kernel|user|hex, default: kernel)");
	output(ctx, "  load-dseg <path> [addr]     Load DSEG binary (addr: kernel|user|hex, default: kernel)");
	output(ctx, "  loadmap <path>              Load additional map file (for multi-file programs)");
	output(ctx, "  loadsrc <path>              Load additional source file (.c or .s)");
	output(ctx, "  loaddom <path>              Load DOM/SEG file header");
	output(ctx, "  run                         Start execution (background)");
	output(ctx, "  stop                        Stop execution");
	output(ctx, "  continue (c/cont)           Continue execution after breakpoint");
	output(ctx, "  symb (symbols) [type]       List symbols (type: all|text|data|bss)");
	output(ctx, "  segments (seg)              Show TEXT/DATA/BSS segment layout");
	output(ctx, "  goto <symbol>               Set PC to symbol address");
	output(ctx, "  msym <symbol> [len]         Memory dump at symbol address");
	output(ctx, "  dsym <symbol> [len]         Disassemble at symbol address");
	output(ctx, "");
	output(ctx, "Source-Level Debugging:");
	output(ctx, "  list [n]                    Show source at PC with n lines context (default 5)");
	output(ctx, "  list [addr]                 Show source at address");
	output(ctx, "  list c [addr]               Show C source (force .c file)");
	output(ctx, "  list asm [addr]             Show assembly source (force .s file)");
	output(ctx, "  l                           Alias for list");
	output(ctx, "");
	output(ctx, "Breakpoints:");
	output(ctx, "  bp [addr] (break/breakpoint) Set breakpoint at address (default: PC)");
	output(ctx, "  bp source <file> <line>     Set breakpoint at source file:line");
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
	output(ctx, "MMU Commands:");
	output(ctx, "  mmu                         Show Program and Data MMU status");
	output(ctx, "  mmu on [program|data]       Enable MMU (both, program only, or data only)");
	output(ctx, "  mmu off [program|data]      Disable MMU (both, program only, or data only)");
	output(ctx, "  mmusetup                    Setup demo MMU configuration for testing");
	output(ctx, "  showmmu                     Show detailed MMU status and configuration");
	output(ctx, "  listpst                     List all configured (non-zero) PST entries");
	output(ctx, "  showpst <psn>               Show PST entry details");
	output(ctx, "  listpcb                     List all domains with configured segments");
	output(ctx, "  showpcb <domain> [seg]      Show PCB capabilities for domain");
	output(ctx, "  phyladr <vaddr> [rw] [id]   Translate virtual to physical address");
	output(ctx, "                              rw: 0=read 1=write, id: 0=data 1=instruction");
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
				uint32_t vaddr = line_addr + j;
				uint32_t paddr = vaddr;
				/* Use MMU translation if enabled */
				if (m->mmu_enabled && m->cpu) {
					paddr = nd500_mmu_translate(m->cpu, vaddr, 0, 0);
				}
				uint8_t b = nd500_bus_read8(m, paddr);
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

/* Memory dump for PROGRAM space (uses instruction MMU path) */
static int cmd_mem_prog(Nd500Machine* m, CmdContext* ctx, char* args) {
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
				uint32_t vaddr = line_addr + j;
				uint32_t paddr = vaddr;
				/* Use MMU translation with is_instruction=1 for program space */
				if (m->mmu_enabled && m->cpu) {
					paddr = nd500_mmu_translate(m->cpu, vaddr, 0, 1);
				}
				uint8_t b = nd500_bus_read8(m, paddr);
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
	} else if (strcmp(sub, "hex") == 0) {
		char* val = strtok(NULL, " \t\r\n");
		int newv;
		if (!val) {
			int cur = nd500_dbg_get_show_hex();
			newv = !cur;
		} else if (strcasecmp(val, "on") == 0) {
			newv = 1;
		} else if (strcasecmp(val, "off") == 0) {
			newv = 0;
		} else {
			error(ctx, "usage: show hex [on|off]");
			return -1;
		}
		nd500_dbg_set_show_hex(newv);
		output(ctx, "show hex: %s", newv ? "on" : "off");
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
	} else if (strcmp(sub, "source") == 0) {
		char* val = strtok(NULL, " \t\r\n");
		int newv = -1;
		if (!val) {
			/* No argument: cycle through modes */
			int cur = nd500_dbg_get_show_source();
			newv = (cur + 1) % 4;  /* 0 -> 1 -> 2 -> 3 -> 0 */
		} else if (strcasecmp(val, "off") == 0) {
			newv = 0;
		} else if (strcasecmp(val, "asm") == 0) {
			newv = 1;
		} else if (strcasecmp(val, "c") == 0) {
			newv = 2;
		} else if (strcasecmp(val, "both") == 0) {
			newv = 3;
		} else {
			error(ctx, "usage: show source [off|asm|c|both]");
			return -1;
		}
		nd500_dbg_set_show_source(newv);
		const char* mode_str[] = {"off", "asm", "c", "both"};
		output(ctx, "show source: %s", mode_str[newv]);
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
	output(ctx, "ST1=%08X ST2=%08X", r.ST1, r.ST2);
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

	/* Special case: set radix */
	if (reg_name && strcasecmp(reg_name, "radix") == 0) {
		if (!value_str) {
			/* Show current radix and options */
			int cur = nd500_dbg_get_radix();
			const char* name = (cur == 1) ? "hex" : (cur == 2) ? "octal" : "decimal";
			output(ctx, "radix: %s (options: decimal, hex, octal)", name);
			return 0;
		}
		if (strcasecmp(value_str, "decimal") == 0 || strcasecmp(value_str, "dec") == 0) {
			nd500_dbg_set_radix(0);
			output(ctx, "radix: decimal");
		} else if (strcasecmp(value_str, "hex") == 0) {
			nd500_dbg_set_radix(1);
			output(ctx, "radix: hex");
		} else if (strcasecmp(value_str, "octal") == 0 || strcasecmp(value_str, "oct") == 0) {
			nd500_dbg_set_radix(2);
			output(ctx, "radix: octal");
		} else {
			error(ctx, "invalid radix: %s (options: decimal, hex, octal)", value_str);
			return -1;
		}
		return 0;
	}

	if (!reg_name || !value_str) {
		error(ctx, "usage: set <register> <value>");
		error(ctx, "       set radix [decimal|hex|octal]");
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
	if (!m || !m->cpu) {
		error(ctx, "no machine or cpu");
		return -1;
	}

	/* Parse file path argument */
	char* filepath = args ? strtok(args, " \t\r\n") : NULL;
	if (!filepath) {
		error(ctx, "usage: load <path-to-aout-file>");
		return -1;
	}

	/* Use unified loading function (auto-loads .map and .s files) */
	uint32_t entry = 0, pc = 0;
	int rc = ndlib_load_aout_with_debug(m, filepath, 1, &entry, &pc);
	if (rc != 0) {
		error(ctx, "failed to load '%s'", filepath);
		return -1;
	}

	/* Report results */
	output(ctx, "loaded: %s", filepath);

	/* Look for initialization script AFTER loading the aout file
	 * This allows the script to configure MMU after data is in physical memory */
	char init_path[512];
	strncpy(init_path, filepath, sizeof(init_path) - 1);
	init_path[sizeof(init_path) - 1] = '\0';

	char* init_ext = strrchr(init_path, '.');
	if (init_ext && *init_ext) {
		/* Replace extension with .init (e.g., kernel.o → kernel.init) */
		strcpy(init_ext, ".init");
	} else {
		/* No extension - append .init to basename (e.g., kernel → kernel.init) */
		strncat(init_path, ".init", sizeof(init_path) - strlen(init_path) - 1);
	}

	/* Execute init script AFTER loading aout (ignore errors - script is optional) */
	nd500_execute_init_script(m, init_path);

	/* Check if .map, .s, and .c files were also loaded */
	char alt_path[512];
	strncpy(alt_path, filepath, sizeof(alt_path) - 1);
	alt_path[sizeof(alt_path) - 1] = '\0';
	char* ext = strrchr(alt_path, '.');
	if (ext && (strcmp(ext, ".o") == 0 || strcmp(ext, ".out") == 0)) {
		/* Check for .map */
		strcpy(ext, ".map");
		FILE* f = fopen(alt_path, "r");
		if (f) {
			fclose(f);
			output(ctx, "loaded: %s", alt_path);
		}

		/* Check for .s */
		strcpy(ext, ".s");
		f = fopen(alt_path, "r");
		if (f) {
			fclose(f);
			output(ctx, "loaded: %s", alt_path);
		}

		/* Check for .c */
		strcpy(ext, ".c");
		f = fopen(alt_path, "r");
		if (f) {
			fclose(f);
			output(ctx, "loaded: %s", alt_path);
		}
	}

	/* Report PC setting */
	if (entry == 0 || entry == 4) {
		if (pc > 0) {
			output(ctx, "PC set to first instruction: 0x%08X", pc);
		} else {
			output(ctx, "PC set to 0 (no map file or first instruction found)");
		}
	} else {
		output(ctx, "PC set to entry point: 0x%08X", pc);
	}

	return 0;
}

/* Default base addresses for PSEG/DSEG loading */
#define PSEG_KERNEL_BASE 0x08000000
#define PSEG_USER_BASE   0xD0000000
#define DSEG_KERNEL_BASE 0x08000000
#define DSEG_USER_BASE   0xD0000000

static int cmd_load_pseg(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no machine or cpu");
		return -1;
	}

	/* Parse arguments: <path> [addr] */
	char* filepath = args ? strtok(args, " \t\r\n") : NULL;
	if (!filepath) {
		error(ctx, "usage: load-pseg <path> [addr]");
		error(ctx, "       addr: kernel (0x%08X) | user (0x%08X) | <hex-addr>",
		      PSEG_KERNEL_BASE, PSEG_USER_BASE);
		error(ctx, "       default: kernel");
		return -1;
	}

	char* addr_arg = strtok(NULL, " \t\r\n");

	/* Determine base address from mode name or explicit address */
	uint32_t base_addr = PSEG_KERNEL_BASE;
	if (addr_arg) {
		if (strcmp(addr_arg, "user") == 0) {
			base_addr = PSEG_USER_BASE;
		} else if (strcmp(addr_arg, "kernel") == 0) {
			base_addr = PSEG_KERNEL_BASE;
		} else {
			base_addr = nd500_cmd_parse_u32(addr_arg, PSEG_KERNEL_BASE);
		}
	}

	/* Load the PSEG file */
	int rc = nd500_load_pseg_file(m, filepath, base_addr);
	if (rc != 0) {
		const char* errmsg = nd500_load_strerror(rc, base_addr, m->memory_size);
		error(ctx, "failed to load PSEG '%s': %s", filepath, errmsg);
		return -1;
	}

	output(ctx, "loaded PSEG: %s at 0x%08X", filepath, base_addr);
	return 0;
}

static int cmd_load_dseg(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no machine or cpu");
		return -1;
	}

	/* Parse arguments: <path> [addr] */
	char* filepath = args ? strtok(args, " \t\r\n") : NULL;
	if (!filepath) {
		error(ctx, "usage: load-dseg <path> [addr]");
		error(ctx, "       addr: kernel (0x%08X) | user (0x%08X) | <hex-addr>",
		      DSEG_KERNEL_BASE, DSEG_USER_BASE);
		error(ctx, "       default: kernel");
		return -1;
	}

	char* addr_arg = strtok(NULL, " \t\r\n");

	/* Determine base address from mode name or explicit address */
	uint32_t base_addr = DSEG_KERNEL_BASE;
	if (addr_arg) {
		if (strcmp(addr_arg, "user") == 0) {
			base_addr = DSEG_USER_BASE;
		} else if (strcmp(addr_arg, "kernel") == 0) {
			base_addr = DSEG_KERNEL_BASE;
		} else {
			base_addr = nd500_cmd_parse_u32(addr_arg, DSEG_KERNEL_BASE);
		}
	}

	/* Load the DSEG file */
	int rc = nd500_load_dseg_file(m, filepath, base_addr);
	if (rc != 0) {
		const char* errmsg = nd500_load_strerror(rc, base_addr, m->memory_size);
		error(ctx, "failed to load DSEG '%s': %s", filepath, errmsg);
		return -1;
	}

	output(ctx, "loaded DSEG: %s at 0x%08X", filepath, base_addr);
	return 0;
}

static int cmd_loadmap(Nd500Machine* m, CmdContext* ctx, char* args) {
	/* Parse file path argument */
	char* filepath = args ? strtok(args, " \t\r\n") : NULL;
	if (!filepath) {
		error(ctx, "usage: loadmap <path-to-map-file>");
		return -1;
	}

	/* Load the map file */
	int rc = ndlib_map_load(filepath);
	if (rc != 0) {
		error(ctx, "failed to load map file: %s", filepath);
		return -1;
	}

	output(ctx, "loaded map: %s", filepath);
	return 0;
}

static int cmd_loadsrc(Nd500Machine* m, CmdContext* ctx, char* args) {
	/* Parse file path argument */
	char* filepath = args ? strtok(args, " \t\r\n") : NULL;
	if (!filepath) {
		error(ctx, "usage: loadsrc <path-to-source-file>");
		return -1;
	}

	/* Check file extension */
	const char* ext = strrchr(filepath, '.');
	if (!ext) {
		error(ctx, "source file must have .c or .s extension");
		return -1;
	}

	if (strcmp(ext, ".c") != 0 && strcmp(ext, ".s") != 0) {
		error(ctx, "source file must have .c or .s extension (got %s)", ext);
		return -1;
	}

	/* Read file contents */
	FILE* f = fopen(filepath, "rb");
	if (!f) {
		error(ctx, "failed to open: %s", filepath);
		return -1;
	}

	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	if (size < 0) {
		fclose(f);
		error(ctx, "failed to read: %s", filepath);
		return -1;
	}
	fseek(f, 0, SEEK_SET);

	char* content = (char*)malloc((size_t)size + 1);
	if (!content) {
		fclose(f);
		error(ctx, "out of memory");
		return -1;
	}

	size_t read = fread(content, 1, (size_t)size, f);
	content[read] = '\0';
	fclose(f);

	/* Extract basename for storage */
	const char* basename = strrchr(filepath, '/');
	if (!basename) basename = strrchr(filepath, '\\');
	basename = basename ? basename + 1 : filepath;

	/* Store the source */
	int rc = ndlib_source_store(basename, content);
	free(content);

	if (rc != 0) {
		error(ctx, "failed to store source: %s", basename);
		return -1;
	}

	output(ctx, "loaded source: %s (stored as %s)", filepath, basename);
	return 0;
}

static int cmd_loaddom(Nd500Machine* m, CmdContext* ctx, char* args) {
	(void)m;  /* DOM loading doesn't require machine state */

	/* Parse file path argument */
	char* filepath = args ? strtok(args, " \t\r\n") : NULL;
	if (!filepath) {
		error(ctx, "usage: loaddom <path-to-dom-or-seg-file>");
		return -1;
	}

	/* Load DOM/SEG header */
	int rc = ndlib_load_dom_header(filepath);
	if (rc != 0) {
		if (rc == -1) {
			error(ctx, "failed to open: %s", filepath);
		} else if (rc == -2) {
			error(ctx, "failed to read header from: %s (file too small?)", filepath);
		} else {
			error(ctx, "failed to load DOM '%s' (error %d)", filepath, rc);
		}
		return -1;
	}

	/* Get header and display basic info */
	const nd500_header_t* hdr = ndlib_get_dom_header();
	if (!hdr) {
		error(ctx, "internal error: header not available after load");
		return -1;
	}

	/* Check if DOM or SEG file */
	int is_dom = ndlib_dom_is_dom_file();

	output(ctx, "Loaded %s: %s", is_dom ? "DOM" : "SEG", filepath);
	output(ctx, "  Linker version: %d.%d", hdr->raw[4], hdr->raw[5]);
	output(ctx, "  Flags: 0x%02X", hdr->raw[6]);
	output(ctx, "  Machine: 0x%02X", hdr->raw[7]);
	output(ctx, "  OS ID: 0x%02X", hdr->raw[8]);
	output(ctx, "  Start addr: 0x%08X", nd500_read32(&hdr->raw[0xD8]));
	output(ctx, "  Restart addr: 0x%08X", nd500_read32(&hdr->raw[0xDC]));

	/* Load segments */
	rc = ndlib_load_dom_segments();
	if (rc != 0) {
		error(ctx, "warning: failed to load segments");
	}

	/* Display segment info */
	output(ctx, "");
	output(ctx, "Segments:");
	int max_segs = is_dom ? 32 : 1;
	int found = 0;

	for (int i = 0; i < max_segs; i++) {
		uint32_t prog_size, prog_addr, data_size, data_addr;
		int is_linked, is_used;

		if (ndlib_dom_get_segment_info(i, &prog_size, &prog_addr,
		                               &data_size, &data_addr,
		                               &is_linked, &is_used) != 0) {
			continue;
		}

		if (!is_used && !is_linked) continue;  /* Skip empty slots */

		found++;

		if (is_linked) {
			output(ctx, "  [%2d] LINKED (external .SEG file)", i);
		} else {
			if (prog_size > 0 || data_size > 0) {
				output(ctx, "  [%2d] PROG: %6u bytes @ 0x%08X  DATA: %6u bytes @ 0x%08X",
				       i, prog_size, prog_addr, data_size, data_addr);
			}
		}
	}

	if (found == 0) {
		output(ctx, "  (no segments found)");
	}

	output(ctx, "");
	output(ctx, "%d segment(s) loaded into memory", ndlib_dom_get_segment_count());

	return 0;
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

static int cmd_list(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	/* Parse arguments: [c|asm] [addr|n] */
	char* a1 = args ? strtok(args, " \t\r\n") : NULL;
	char* a2 = a1 ? strtok(NULL, " \t\r\n") : NULL;

	int force_c = 0, force_asm = 0;
	uint32_t addr = m->cpu->PC;
	int context_lines = 5;

	/* Check first argument for c/asm forcing */
	if (a1 && (strcasecmp(a1, "c") == 0 || strcasecmp(a1, "asm") == 0)) {
		if (strcasecmp(a1, "c") == 0) force_c = 1;
		else force_asm = 1;
		/* Second argument becomes address or context */
		if (a2) {
			addr = nd500_cmd_parse_u32(a2, m->cpu->PC);
		}
	} else if (a1) {
		/* First argument is address or context lines */
		uint32_t val = nd500_cmd_parse_u32(a1, m->cpu->PC);
		/* If value is small (< 100), treat as context lines, else as address */
		if (val < 100) {
			context_lines = (int)val;
		} else {
			addr = val;
		}
	}

	/* Get source file and line at this address */
	const char* c_file = NULL;
	const char* s_file = NULL;
	int c_line = 0;
	int s_line = 0;

	int has_c = ndlib_symbols_get_c_mapping(addr, &c_file, &c_line);
	int has_s = ndlib_symbols_get_s_mapping(addr, &s_file, &s_line);

	/* Determine which source to show based on forcing and availability */
	const char* source_file = NULL;
	int source_line = 0;

	if (force_c) {
		/* User explicitly requested C source */
		if (!has_c) {
			if (has_s) {
				error(ctx, "no C source at address 0x%08X (only assembly %s available)", addr, s_file);
			} else {
				error(ctx, "no C source at address 0x%08X", addr);
			}
			return -1;
		}
		source_file = c_file;
		source_line = c_line;
	} else if (force_asm) {
		/* User explicitly requested assembly source */
		if (!has_s) {
			if (has_c) {
				error(ctx, "no assembly source at address 0x%08X (only C %s available)", addr, c_file);
			} else {
				error(ctx, "no assembly source at address 0x%08X", addr);
			}
			return -1;
		}
		source_file = s_file;
		source_line = s_line;
	} else {
		/* Default: prefer C, fall back to assembly */
		if (has_c) {
			source_file = c_file;
			source_line = c_line;
		} else if (has_s) {
			source_file = s_file;
			source_line = s_line;
		} else {
			error(ctx, "no source mapping at address 0x%08X", addr);
			return -1;
		}
	}

	/* Get total line count and calculate range */
	int total_lines = ndlib_source_count_lines(source_file);
	if (total_lines == 0) {
		error(ctx, "source file not loaded: %s", source_file);
		return -1;
	}

	int start_line = source_line - context_lines;
	int end_line = source_line + context_lines;
	if (start_line < 1) start_line = 1;
	if (end_line > total_lines) end_line = total_lines;

	/* Display header */
	output(ctx, "=== %s (lines %d-%d) ===", source_file, start_line, end_line);

	/* Display source lines */
	for (int line = start_line; line <= end_line; line++) {
		const char* line_text = ndlib_source_get_line(source_file, line);
		if (line_text) {
			/* Check if this line has any breakpoints */
			uint32_t line_addrs[16];
			int addr_count = ndlib_symbols_get_addrs_for_line(source_file, line, line_addrs, 16);
			int has_breakpoint = 0;
			for (int i = 0; i < addr_count; i++) {
				if (nd500_dbg_has_breakpoint_at(m, line_addrs[i])) {
					has_breakpoint = 1;
					break;
				}
			}

			/* Build marker: ● for breakpoint, → for current line */
			if (has_breakpoint && line == source_line) {
				/* Both breakpoint and current line */
				output(ctx, "●→%4d  %s", line, line_text);
			} else if (has_breakpoint) {
				/* Breakpoint only */
				output(ctx, "● %4d  %s", line, line_text);
			} else if (line == source_line) {
				/* Current line only */
				output(ctx, " →%4d  %s", line, line_text);
			} else {
				/* Neither */
				output(ctx, "  %4d  %s", line, line_text);
			}
		}
	}

	return 0;
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
	} else if (strcasecmp(a1, "source") == 0 || strcasecmp(a1, "src") == 0) {
		/* Set breakpoint by source file and line number */
		if (!a2) {
			error(ctx, "usage: bp source <file> <line>");
			return -1;
		}
		char* a3 = strtok(NULL, " \t\r\n");
		if (!a3) {
			error(ctx, "usage: bp source <file> <line>");
			return -1;
		}

		const char* filename = a2;
		int line = (int)nd500_cmd_parse_u32(a3, 0);

		/* Look up address for this source location */
		uint32_t addr = 0;
		if (ndlib_symbols_addr_for_line(filename, line, &addr) != 0) {
			error(ctx, "no code found at %s:%d", filename, line);
			return -1;
		}

		/* Set breakpoint at the found address */
		int bp_id = bp_add(m->bp_mgr, addr, false);
		if (bp_id >= 0) {
			output(ctx, "breakpoint %d set at %s:%d (address 0x%08X)", bp_id, filename, line, addr);
		}
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

/* ═══════════════════════════════════════════════════════ */
/* MMU COMMAND HANDLERS */
/* ═══════════════════════════════════════════════════════ */

static int cmd_mmu(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no machine");
		return -1;
	}

	char* subcmd = args ? strtok(args, " \t\r\n") : NULL;

	if (!subcmd) {
		/* No argument - show current status */
		int prog_enabled = nd500_mmu_is_program_enabled(m->cpu);
		int data_enabled = nd500_mmu_is_data_enabled(m->cpu);
		output(ctx, "Program MMU: %s", prog_enabled ? "enabled" : "disabled");
		output(ctx, "Data MMU:    %s", data_enabled ? "enabled" : "disabled");
		return 0;
	}

	/* Parse type argument (optional) */
	char* type = strtok(NULL, " \t\r\n");

	if (strcasecmp(subcmd, "on") == 0 || strcasecmp(subcmd, "enable") == 0) {
		if (!type) {
			/* Enable both */
			nd500_machine_enable_mmu(m);
			output(ctx, "Program and Data MMU enabled");
		} else if (strcasecmp(type, "program") == 0 || strcasecmp(type, "prog") == 0) {
			nd500_mmu_enable_program(m->cpu);
			output(ctx, "Program MMU enabled (PMON)");
		} else if (strcasecmp(type, "data") == 0) {
			nd500_mmu_enable_data(m->cpu);
			output(ctx, "Data MMU enabled (DMON)");
		} else {
			error(ctx, "usage: mmu on|enable [program|data]");
			return -1;
		}
	} else if (strcasecmp(subcmd, "off") == 0 || strcasecmp(subcmd, "disable") == 0) {
		if (!type) {
			/* Disable both */
			nd500_machine_disable_mmu(m);
			output(ctx, "Program and Data MMU disabled");
		} else if (strcasecmp(type, "program") == 0 || strcasecmp(type, "prog") == 0) {
			nd500_mmu_disable_program(m->cpu);
			output(ctx, "Program MMU disabled (PMOF)");
		} else if (strcasecmp(type, "data") == 0) {
			nd500_mmu_disable_data(m->cpu);
			output(ctx, "Data MMU disabled (DMOF)");
		} else {
			error(ctx, "usage: mmu off|disable [program|data]");
			return -1;
		}
	} else if (strcasecmp(subcmd, "identity") == 0) {
		/* mmu identity <start> <end> <flags> */
		char* start_str = type;  /* type was consumed as first token */
		char* end_str = strtok(NULL, " \t\r\n");
		char* flags_str = strtok(NULL, " \t\r\n");

		if (!start_str || !end_str || !flags_str) {
			error(ctx, "usage: mmu identity <start> <end> <flags>");
			error(ctx, "  example: mmu identity 0x00000000 0x00FFFFFF rwx");
			return -1;
		}

		uint32_t start_addr = nd500_cmd_parse_u32(start_str, 0);
		uint32_t end_addr = nd500_cmd_parse_u32(end_str, 0);

		if (end_addr < start_addr) {
			error(ctx, "end address must be >= start address");
			return -1;
		}

		/* Parse flags */
		int has_read = strchr(flags_str, 'r') || strchr(flags_str, 'R');
		int has_write = strchr(flags_str, 'w') || strchr(flags_str, 'W');
		int has_exec = strchr(flags_str, 'x') || strchr(flags_str, 'X');

		/* Calculate number of pages needed (each page is 2KB) */
		uint32_t size = end_addr - start_addr + 1;
		uint32_t num_pages = (size + NBPG - 1) / NBPG;  /* Round up */
		uint32_t start_pfn = start_addr >> PGSHIFT;

		output(ctx, "Identity mapping 0x%08X-0x%08X (%s%s%s)",
			start_addr, end_addr,
			has_read ? "r" : "-",
			has_write ? "w" : "-",
			has_exec ? "x" : "-");
		output(ctx, "  Requires %u pages (2KB each)", num_pages);

		/* Calculate segment range (each segment is 128MB) */
		uint32_t start_seg = start_addr >> SGSHIFT;
		uint32_t end_seg = end_addr >> SGSHIFT;

		/* Page table allocation base (1MB physical address, well past kernel) */
		uint32_t page_table_base = 0x00100000;  /* 1MB */
		uint32_t next_psn = 1;  /* Next available PSN (start from 1, PSN 0 reserved for "no capability") */

		/* Create one PST entry per segment using PS_ASI (single-level paging) */
		for (uint32_t seg = start_seg; seg <= end_seg && seg < MAXSEG; seg++) {
			/* Calculate page range for this segment */
			uint32_t seg_start_addr = seg << SGSHIFT;
			uint32_t seg_end_addr = ((seg + 1) << SGSHIFT) - 1;

			/* Clip to requested range */
			if (seg_start_addr < start_addr) seg_start_addr = start_addr;
			if (seg_end_addr > end_addr) seg_end_addr = end_addr;

			uint32_t seg_start_page = seg_start_addr >> PGSHIFT;
			uint32_t seg_end_page = seg_end_addr >> PGSHIFT;
			uint32_t seg_num_pages = seg_end_page - seg_start_page + 1;

			/* Allocate page table for this segment */
			uint32_t page_table_addr = page_table_base;
			page_table_base += seg_num_pages * 4;  /* 4 bytes per PTE */

			/* Create PTEs in the page table (identity mapping: page i → PFN i) */
			for (uint32_t i = 0; i < seg_num_pages; i++) {
				uint32_t page_num = seg_start_page + i;
				uint32_t pte_addr = page_table_addr + (i * 4);

				/* PTE format: [31:2]=PFN, [1]=valid/present, [0]=protection */
				uint8_t protection = (has_write) ? 0 : 1;  /* 0=writable, 1=read-only */
				uint32_t pte_value = (page_num << 2) | (1 << 1) | protection;  /* Set valid bit */

				/* Write PTE to physical memory */
				nd500_bus_write8(m, pte_addr + 0, (pte_value >> 0) & 0xFF);
				nd500_bus_write8(m, pte_addr + 1, (pte_value >> 8) & 0xFF);
				nd500_bus_write8(m, pte_addr + 2, (pte_value >> 16) & 0xFF);
				nd500_bus_write8(m, pte_addr + 3, (pte_value >> 24) & 0xFF);
			}

			/* Create PST entry pointing to page table (PS_ASI mode) */
			uint32_t page_table_pfn = page_table_addr >> PGSHIFT;
			nd500_mmu_set_pst_entry(m->cpu, next_psn, PS_ASI, page_table_pfn);

			output(ctx, "  Segment %u: PST[%u] → page table at 0x%08X (%u pages)",
				seg, next_psn, page_table_addr, seg_num_pages);

			/* Set capabilities for this segment */
			if (has_exec) {
				uint16_t pc = next_psn | PC_DIR;
				nd500_mmu_set_program_capability(m->cpu, 0, seg, pc);
				output(ctx, "    Prog capability: PSN %u", next_psn);
			}

			if (has_read || has_write) {
				uint16_t dc = next_psn;
				if (!has_write) {  /* DC_WRP=1 means write-protected (read-only) */
					dc |= DC_WRP;
				}
				nd500_mmu_set_data_capability(m->cpu, 0, seg, dc);
				output(ctx, "    Data capability: PSN %u %s", next_psn,
					has_write ? "(writable)" : "(read-only)");
			}

			next_psn++;
		}

		output(ctx, "  Identity mapping complete: segments %u-%u, %u PST entries used",
			start_seg, end_seg, next_psn);

	} else if (strcasecmp(subcmd, "map") == 0) {
		/* mmu map <vstart> <vend> <pstart> <flags> */
		char* vstart_str = type;  /* type was consumed as first token */
		char* vend_str = strtok(NULL, " \t\r\n");
		char* pstart_str = strtok(NULL, " \t\r\n");
		char* flags_str = strtok(NULL, " \t\r\n");

		if (!vstart_str || !vend_str || !pstart_str || !flags_str) {
			error(ctx, "usage: mmu map <vstart> <vend> <pstart> <flags>");
			error(ctx, "  example: mmu map 0xE8000000 0xE8FFFFFF 0x00000000 rw");
			return -1;
		}

		uint32_t vstart_addr = nd500_cmd_parse_u32(vstart_str, 0);
		uint32_t vend_addr = nd500_cmd_parse_u32(vend_str, 0);
		uint32_t pstart_addr = nd500_cmd_parse_u32(pstart_str, 0);

		if (vend_addr < vstart_addr) {
			error(ctx, "virtual end address must be >= virtual start address");
			return -1;
		}

		/* Parse flags */
		int has_read = strchr(flags_str, 'r') || strchr(flags_str, 'R');
		int has_write = strchr(flags_str, 'w') || strchr(flags_str, 'W');
		int has_exec = strchr(flags_str, 'x') || strchr(flags_str, 'X');

		/* Calculate number of pages needed */
		uint32_t vsize = vend_addr - vstart_addr + 1;
		uint32_t num_pages = (vsize + NBPG - 1) / NBPG;  /* Round up */

		output(ctx, "Mapping virtual 0x%08X-0x%08X → physical 0x%08X (%s%s%s)",
			vstart_addr, vend_addr, pstart_addr,
			has_read ? "r" : "-",
			has_write ? "w" : "-",
			has_exec ? "x" : "-");
		output(ctx, "  Requires %u pages (2KB each)", num_pages);

		/* Calculate segment range */
		uint32_t start_vseg = vstart_addr >> SGSHIFT;
		uint32_t end_vseg = vend_addr >> SGSHIFT;

		/* Page table allocation base (start after identity mapping tables) */
		uint32_t page_table_base = 0x00200000;  /* 2MB physical address */
		uint32_t next_psn = 10;  /* Start at PSN 10 to avoid conflict with identity mapping */

		/* Create one PST entry per segment using PS_ASI (single-level paging) */
		for (uint32_t seg = start_vseg; seg <= end_vseg && seg < MAXSEG; seg++) {
			/* Calculate page range for this segment */
			uint32_t seg_start_vaddr = seg << SGSHIFT;
			uint32_t seg_end_vaddr = ((seg + 1) << SGSHIFT) - 1;

			/* Clip to requested range */
			if (seg_start_vaddr < vstart_addr) seg_start_vaddr = vstart_addr;
			if (seg_end_vaddr > vend_addr) seg_end_vaddr = vend_addr;

			uint32_t seg_start_vpage = seg_start_vaddr >> PGSHIFT;
			uint32_t seg_end_vpage = seg_end_vaddr >> PGSHIFT;
			uint32_t seg_num_pages = seg_end_vpage - seg_start_vpage + 1;

			/* Calculate corresponding physical pages */
			uint32_t offset_in_mapping = seg_start_vaddr - vstart_addr;
			uint32_t seg_start_ppage = (pstart_addr + offset_in_mapping) >> PGSHIFT;

			/* Allocate page table for this segment */
			uint32_t page_table_addr = page_table_base;
			page_table_base += seg_num_pages * 4;  /* 4 bytes per PTE */

			/* Create PTEs in the page table */
			for (uint32_t i = 0; i < seg_num_pages; i++) {
				uint32_t phys_page_num = seg_start_ppage + i;
				uint32_t pte_addr = page_table_addr + (i * 4);

				/* PTE format: [31:2]=PFN, [1]=valid/present, [0]=protection */
				uint8_t protection = (has_write) ? 0 : 1;  /* 0=writable, 1=read-only */
				uint32_t pte_value = (phys_page_num << 2) | (1 << 1) | protection;  /* Set valid bit */

				/* Write PTE to physical memory */
				nd500_bus_write8(m, pte_addr + 0, (pte_value >> 0) & 0xFF);
				nd500_bus_write8(m, pte_addr + 1, (pte_value >> 8) & 0xFF);
				nd500_bus_write8(m, pte_addr + 2, (pte_value >> 16) & 0xFF);
				nd500_bus_write8(m, pte_addr + 3, (pte_value >> 24) & 0xFF);;
			}

			/* Create PST entry pointing to page table (PS_ASI mode) */
			uint32_t page_table_pfn = page_table_addr >> PGSHIFT;
			nd500_mmu_set_pst_entry(m->cpu, next_psn, PS_ASI, page_table_pfn);

			output(ctx, "  Segment %u: PST[%u] → page table at 0x%08X (%u pages)",
				seg, next_psn, page_table_addr, seg_num_pages);

			/* Set capabilities for this segment */
			if (has_exec) {
				uint16_t pc = next_psn | PC_DIR;
				nd500_mmu_set_program_capability(m->cpu, 0, seg, pc);
				output(ctx, "    Prog capability: PSN %u", next_psn);
			}

			if (has_read || has_write) {
				uint16_t dc = next_psn;
				if (!has_write) {  /* DC_WRP=1 means write-protected (read-only) */
					dc |= DC_WRP;
				}
				nd500_mmu_set_data_capability(m->cpu, 0, seg, dc);
				output(ctx, "    Data capability: PSN %u %s", next_psn,
					has_write ? "(writable)" : "(read-only)");
			}

			next_psn++;
		}

		output(ctx, "  Mapping complete: segments %u-%u, %u PST entries used",
			start_vseg, end_vseg, next_psn - 10);

	} else {
		error(ctx, "usage: mmu [on|off|enable|disable|identity|map] ...");
		return -1;
	}

	return 0;
}

static int cmd_showmmu(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	int prog_enabled = nd500_mmu_is_program_enabled(m->cpu);
	int data_enabled = nd500_mmu_is_data_enabled(m->cpu);

	output(ctx, "=== MMU STATUS ===");
	output(ctx, "Program MMU (PMON/PMOF): %s", prog_enabled ? "enabled" : "disabled");
	output(ctx, "Data MMU (DMON/DMOF):    %s", data_enabled ? "enabled" : "disabled");
	output(ctx, "");
	output(ctx, "MMU Registers:");
	output(ctx, "  PSTP    = 0x%08X  (Physical Segment Table Pointer)", m->cpu->PSTP);
	output(ctx, "  DITBASE = 0x%08X  (Domain Information Table Base)", m->cpu->DITBASE);
	output(ctx, "  CED     = 0x%08X  (Current Executing Domain)", m->cpu->CED);
	output(ctx, "  CAD     = 0x%08X  (Current Alternative Domain)", m->cpu->CAD);
	output(ctx, "  PS      = 0x%08X  (Process Segment)", m->cpu->PS);
	output(ctx, "");

	/* Count configured PST entries */
	int pst_count = 0;
	for (uint32_t psn = 0; psn < MAX_PST; psn++) {
		PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(m->cpu, psn);
		if (pst.index_mode != 0 || pst.physical_pfn != 0) {
			pst_count++;
		}
	}

	/* Count configured PCB domains/segments */
	int domain_count = 0;
	int segment_count = 0;
	for (uint32_t domain = 0; domain < MAXDOM; domain++) {
		int has_segments = 0;
		for (int seg = 0; seg < 32; seg++) {
			uint16_t pc = nd500_mmu_get_program_capability(m->cpu, domain, seg);
			uint16_t dc = nd500_mmu_get_data_capability(m->cpu, domain, seg);
			if (pc != 0 || dc != 0) {
				if (!has_segments) {
					domain_count++;
					has_segments = 1;
				}
				segment_count++;
			}
		}
	}

	output(ctx, "PST: %d configured entries (of %d max)", pst_count, MAX_PST);
	output(ctx, "PCB: %d domains with %d segments (of %d domains max)", domain_count, segment_count, MAXDOM);
	output(ctx, "Page size: %d bytes", NBPG);
	output(ctx, "");
	output(ctx, "Use 'listpst' to see all configured PST entries");
	output(ctx, "Use 'listpcb' to see all configured domains and segments");

	return 0;
}

static int cmd_showpst(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	char* psn_str = args ? strtok(args, " \t\r\n") : NULL;
	if (!psn_str) {
		error(ctx, "usage: showpst <psn>");
		return -1;
	}

	uint32_t psn = nd500_cmd_parse_u32(psn_str, 0);
	if (psn >= MAX_PST) {
		error(ctx, "PSN out of range (0-%d)", MAX_PST - 1);
		return -1;
	}

	PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(m->cpu, psn);

	output(ctx, "=== PST Entry %u ===", psn);
	output(ctx, "Index Mode:    %u (%s)", pst.index_mode,
		pst.index_mode == PS_AZI ? "PS_AZI - Direct" :
		pst.index_mode == PS_ASI ? "PS_ASI - Single-level paging" :
		pst.index_mode == PS_ADI ? "PS_ADI - Two-level paging" : "Unknown");
	output(ctx, "Physical PFN:  0x%04X (Physical address: 0x%08X)",
		pst.physical_pfn, pst.physical_pfn << PGSHIFT);

	if (pst.index_mode == PS_AZI) {
		output(ctx, "Direct mapping: segment maps to physical frame 0x%04X", pst.physical_pfn);
	} else if (pst.index_mode == PS_ASI) {
		output(ctx, "Page table at: 0x%08X (single-level)", pst.physical_pfn << PGSHIFT);
	} else if (pst.index_mode == PS_ADI) {
		output(ctx, "L1 page table at: 0x%08X (two-level)", pst.physical_pfn << PGSHIFT);
	}

	return 0;
}

static int cmd_showpcb(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	char* domain_str = args ? strtok(args, " \t\r\n") : NULL;
	char* seg_str = domain_str ? strtok(NULL, " \t\r\n") : NULL;

	if (!domain_str) {
		error(ctx, "usage: showpcb <domain> [segment]");
		return -1;
	}

	uint32_t domain = nd500_cmd_parse_u32(domain_str, 0);
	if (domain >= MAXDOM) {
		error(ctx, "Domain out of range (0-%d)", MAXDOM - 1);
		return -1;
	}

	if (seg_str) {
		/* Show specific segment */
		uint32_t seg = nd500_cmd_parse_u32(seg_str, 0);
		if (seg >= 32) {
			error(ctx, "Segment out of range (0-31)");
			return -1;
		}

		uint16_t pc = nd500_mmu_get_program_capability(m->cpu, domain, seg);
		uint16_t dc = nd500_mmu_get_data_capability(m->cpu, domain, seg);

		output(ctx, "=== PCB Domain %u Segment %u ===", domain, seg);
		output(ctx, "Program Capability: 0x%04X", pc);
		output(ctx, "  PSN:     %u (0x%03X)", pc & PC_PSN, pc & PC_PSN);
		output(ctx, "  DIR bit: %u (%s)", (pc & PC_DIR) ? 1 : 0, (pc & PC_DIR) ? "Direct mapped" : "Not direct");
		output(ctx, "");
		output(ctx, "Data Capability:    0x%04X", dc);
		output(ctx, "  PSN:     %u (0x%03X)", dc & DC_PSN, dc & DC_PSN);
		output(ctx, "  WRP bit: %u (%s)", (dc & DC_WRP) ? 1 : 0, (dc & DC_WRP) ? "Write-protected" : "Writable");
		output(ctx, "  PAC bit: %u (%s)", (dc & DC_PAC) ? 1 : 0, (dc & DC_PAC) ? "User accessible" : "Kernel only");
	} else {
		/* Show all segments for domain */
		output(ctx, "=== PCB Domain %u ===", domain);
		output(ctx, "Seg  Prog Cap  Data Cap");
		output(ctx, "---  --------  --------");

		for (int seg = 0; seg < 32; seg++) {
			uint16_t pc = nd500_mmu_get_program_capability(m->cpu, domain, seg);
			uint16_t dc = nd500_mmu_get_data_capability(m->cpu, domain, seg);

			/* Only show non-zero entries */
			if (pc != 0 || dc != 0) {
				output(ctx, "%3d  %04X      %04X", seg, pc, dc);
			}
		}
	}

	return 0;
}

static int cmd_phyladr(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	if (!nd500_machine_mmu_is_enabled(m)) {
		error(ctx, "MMU is disabled - addresses are already physical");
		return -1;
	}

	char* vaddr_str = args ? strtok(args, " \t\r\n") : NULL;
	char* rw_str = vaddr_str ? strtok(NULL, " \t\r\n") : NULL;
	char* id_str = rw_str ? strtok(NULL, " \t\r\n") : NULL;

	if (!vaddr_str) {
		error(ctx, "usage: phyladr <vaddr> [is_write] [is_instruction]");
		error(ctx, "  is_write: 0=read (default), 1=write");
		error(ctx, "  is_instruction: 0=data (default), 1=instruction");
		return -1;
	}

	uint32_t vaddr = nd500_cmd_parse_u32(vaddr_str, 0);
	int is_write = rw_str ? (int)nd500_cmd_parse_u32(rw_str, 0) : 0;
	int is_instruction = id_str ? (int)nd500_cmd_parse_u32(id_str, 0) : 0;

	/* Extract address components */
	int segment = (vaddr >> 27) & 0x1F;
	int page = (vaddr >> PGSHIFT) & 0xFFFF;
	int offset = vaddr & (NBPG - 1);

	output(ctx, "=== Virtual Address Translation ===");
	output(ctx, "Virtual Address: 0x%08X", vaddr);
	output(ctx, "  Segment: %d (0x%02X)", segment, segment);
	output(ctx, "  Page:    %d (0x%04X)", page, page);
	output(ctx, "  Offset:  %d (0x%03X)", offset, offset);
	output(ctx, "");
	output(ctx, "Access Type:");
	output(ctx, "  %s access", is_write ? "Write" : "Read");
	output(ctx, "  %s space", is_instruction ? "Instruction" : "Data");
	output(ctx, "");
	output(ctx, "Current Domain:");
	output(ctx, "  CAD (Alternative): %u", m->cpu->CAD);
	output(ctx, "  CED (Executing):   %u", m->cpu->CED);
	output(ctx, "");

	/* Perform translation */
	uint32_t paddr = nd500_mmu_translate(m->cpu, vaddr, is_write, is_instruction);

	if (paddr == 0 && vaddr != 0) {
		output(ctx, "Translation FAILED (trap would occur)");
		output(ctx, "  Possible causes:");
		output(ctx, "  - Invalid capability (null)");
		output(ctx, "  - Protection violation");
		output(ctx, "  - Page fault (PFN=0)");
	} else {
		output(ctx, "Physical Address: 0x%08X", paddr);
		output(ctx, "  PFN:    0x%04X", paddr >> PGSHIFT);
		output(ctx, "  Offset: 0x%03X", paddr & (NBPG - 1));
	}

	return 0;
}

static int cmd_mmusetup(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	output(ctx, "Setting up MMU with 3 domains: Kernel (0) + User1 (1) + User2 (2)");
	output(ctx, "Each domain gets 256KB code + 256KB data (128 pages each)");
	output(ctx, "");
	output(ctx, "Physical Memory Layout:");
	output(ctx, "  0x00000000-0x0003FFFF: Domain 0 (Kernel) Code (256KB = 128 pages)");
	output(ctx, "  0x00040000-0x0007FFFF: Domain 0 (Kernel) Data (256KB = 128 pages)");
	output(ctx, "  0x00080000-0x000BFFFF: Domain 1 (User1) Code (256KB = 128 pages)");
	output(ctx, "  0x000C0000-0x000FFFFF: Domain 1 (User1) Data (256KB = 128 pages)");
	output(ctx, "  0x00100000-0x0013FFFF: Domain 2 (User2) Code (256KB = 128 pages)");
	output(ctx, "  0x00140000-0x0017FFFF: Domain 2 (User2) Data (256KB = 128 pages)");
	output(ctx, "  0x00180000-0x00FFFFFF: Available (~14.5 MB)");
	output(ctx, "");

	/* ═══════════════════════════════════════════════════════
	 * PST CONFIGURATION - Create 128 contiguous pages per region
	 * Each domain needs 256 PST entries (128 for code + 128 for data)
	 * Total: 768 PST entries
	 * ═══════════════════════════════════════════════════════ */
	output(ctx, "=== PST Configuration ===");
	output(ctx, "Creating 768 PST entries (256 per domain)...");

	/* Domain 0 (Kernel) Code: PSN 0-127 → Physical 0x00000000-0x0003FFFF */
	for (uint32_t i = 0; i < 128; i++) {
		nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);  /* PFN = PSN for direct mapping */
	}
	output(ctx, "PST[0-127]     = Domain 0 kernel code (phys 0x00000000-0x0003FFFF)");

	/* Domain 0 (Kernel) Data: PSN 128-255 → Physical 0x00040000-0x0007FFFF */
	for (uint32_t i = 128; i < 256; i++) {
		nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);
	}
	output(ctx, "PST[128-255]   = Domain 0 kernel data (phys 0x00040000-0x0007FFFF)");

	/* Domain 1 (User1) Code: PSN 256-383 → Physical 0x00080000-0x000BFFFF */
	for (uint32_t i = 256; i < 384; i++) {
		nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);
	}
	output(ctx, "PST[256-383]   = Domain 1 user1 code (phys 0x00080000-0x000BFFFF)");

	/* Domain 1 (User1) Data: PSN 384-511 → Physical 0x000C0000-0x000FFFFF */
	for (uint32_t i = 384; i < 512; i++) {
		nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);
	}
	output(ctx, "PST[384-511]   = Domain 1 user1 data (phys 0x000C0000-0x000FFFFF)");

	/* Domain 2 (User2) Code: PSN 512-639 → Physical 0x00100000-0x0013FFFF */
	for (uint32_t i = 512; i < 640; i++) {
		nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);
	}
	output(ctx, "PST[512-639]   = Domain 2 user2 code (phys 0x00100000-0x0013FFFF)");

	/* Domain 2 (User2) Data: PSN 640-767 → Physical 0x00140000-0x0017FFFF */
	for (uint32_t i = 640; i < 768; i++) {
		nd500_mmu_set_pst_entry(m->cpu, i, PS_AZI, i);
	}
	output(ctx, "PST[640-767]   = Domain 2 user2 data (phys 0x00140000-0x0017FFFF)");

	output(ctx, "");
	output(ctx, "=== PCB Configuration ===");
	output(ctx, "Mapping 128 consecutive segment entries per domain...");
	output(ctx, "");

	/* Domain 0 (Kernel): Virtual segment 0 onwards */
	output(ctx, "Domain 0 (Kernel):");
	/* Code segments 0-127: Each segment i maps to PSN i (phys 0x00000000+) */
	for (uint32_t seg = 0; seg < 128; seg++) {
		nd500_mmu_set_program_capability(m->cpu, 0, seg, seg | PC_DIR);
	}
	output(ctx, "  Prog segments [0-127]   → PSN [0-127]   (virtual 0x00000000-0x3F800000)");

	/* Data segments 0-127: Each segment i maps to PSN 128+i (phys 0x00040000+) */
	for (uint32_t seg = 0; seg < 128; seg++) {
		nd500_mmu_set_data_capability(m->cpu, 0, seg, (128 + seg));
	}
	output(ctx, "  Data segments [0-127]   → PSN [128-255] (virtual 0x00000000-0x3F800000)");

	/* Special: Segment 31 for Domain 0 = ND-100 Other Machine (INDIRECT + OMC) */
	/* Bit 15 = 1 (INDIRECT), Bit 14 = 1 (OMC), Domain=0, Segment=1 */
	nd500_mmu_set_program_capability(m->cpu, 0, 31, PC_IND | PC_OMC | (0 << 5) | 1);
	output(ctx, "  Prog segment 31         → INDIRECT OMC Domain=0 Seg=1 (ND-100)");

	output(ctx, "");
	output(ctx, "Domain 1 (User1):");
	/* Code segments 0-127: Each segment i maps to PSN 256+i (phys 0x00080000+) */
	for (uint32_t seg = 0; seg < 128; seg++) {
		nd500_mmu_set_program_capability(m->cpu, 1, seg, (256 + seg) | PC_DIR);
	}
	output(ctx, "  Prog segments [0-127]   → PSN [256-383] (virtual 0x00000000-0x3F800000)");

	/* Data segments 0-127: Each segment i maps to PSN 384+i (phys 0x000C0000+) */
	for (uint32_t seg = 0; seg < 128; seg++) {
		nd500_mmu_set_data_capability(m->cpu, 1, seg, (384 + seg) | DC_PAC);
	}
	output(ctx, "  Data segments [0-127]   → PSN [384-511] (virtual 0x00000000-0x3F800000)");

	/* Special: Segment 31 for Domain 1 = Link to Kernel (INDIRECT, no OMC) */
	/* Bit 15 = 1 (INDIRECT), Bit 14 = 0 (no OMC), Domain=0, Segment=1 */
	nd500_mmu_set_program_capability(m->cpu, 1, 31, PC_IND | (0 << 5) | 1);
	output(ctx, "  Prog segment 31         → INDIRECT Domain=0 Seg=1 (→ Kernel)");

	output(ctx, "");
	output(ctx, "Domain 2 (User2):");
	/* Code segments 0-127: Each segment i maps to PSN 512+i (phys 0x00100000+) */
	for (uint32_t seg = 0; seg < 128; seg++) {
		nd500_mmu_set_program_capability(m->cpu, 2, seg, (512 + seg) | PC_DIR);
	}
	output(ctx, "  Prog segments [0-127]   → PSN [512-639] (virtual 0x00100000-0x0013FFFF)");

	/* Data segments 0-127: Each segment i maps to PSN 640+i (phys 0x00140000+) */
	for (uint32_t seg = 0; seg < 128; seg++) {
		nd500_mmu_set_data_capability(m->cpu, 2, seg, (640 + seg) | DC_PAC);
	}
	output(ctx, "  Data segments [0-127]   → PSN [640-767] (virtual 0x00140000-0x0017FFFF)");

	/* Special: Segment 31 for Domain 2 = Link to Kernel (INDIRECT, no OMC) */
	/* Bit 15 = 1 (INDIRECT), Bit 14 = 0 (no OMC), Domain=0, Segment=1 */
	nd500_mmu_set_program_capability(m->cpu, 2, 31, PC_IND | (0 << 5) | 1);
	output(ctx, "  Prog segment 31         → INDIRECT Domain=0 Seg=1 (→ Kernel)");

	output(ctx, "");
	output(ctx, "=== MMU Registers ===");
	m->cpu->PSTP = 0x00100000;
	m->cpu->DITBASE = 0x00200000;
	m->cpu->CAD = 0;
	m->cpu->CED = 0;
	m->cpu->PS = 0;
	output(ctx, "PSTP    = 0x00100000");
	output(ctx, "DITBASE = 0x00200000");
	output(ctx, "CAD     = 0 (Alternative Domain - kernel)");
	output(ctx, "CED     = 0 (Executing Domain - kernel)");
	output(ctx, "PS      = 0 (Process Segment)");

	/* Enable MMU */
	output(ctx, "");
	nd500_machine_enable_mmu(m);
	output(ctx, "MMU ENABLED - Virtual memory now active!");

	output(ctx, "");
	output(ctx, "Virtual Memory Layout (each domain has 256KB code + 256KB data):");
	output(ctx, "  Domain 0 (Kernel): Virtual 0x00000000-0x3F800000 → Phys 0x00000000-0x0007FFFF");
	output(ctx, "  Domain 1 (User1):  Virtual 0x00000000-0x3F800000 → Phys 0x00080000-0x000FFFFF");
	output(ctx, "  Domain 2 (User2):  Virtual 0x00000000-0x3F800000 → Phys 0x00100000-0x0017FFFF");
	output(ctx, "");
	output(ctx, "Configuration complete! You can now:");
	output(ctx, "  - Load PSEG/DSEG files to any virtual address 0x00000000-0x3F800000");
	output(ctx, "  - Switch domains using 'set CAD <domain>' or 'set CED <domain>'");
	output(ctx, "  - Use 'showmmu' to view MMU status");
	output(ctx, "  - Use 'phyladr <vaddr>' to test address translation");
	output(ctx, "  - Use 'listpst' to see all 768 configured PST entries");
	output(ctx, "  - Use 'listpcb' to see domain configurations");

	return 0;
}

static int cmd_listpst(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	output(ctx, "=== Configured PST Entries ===");
	output(ctx, "PSN   Mode  PFN     Physical Address");
	output(ctx, "----  ----  ------  ----------------");

	int count = 0;
	for (uint32_t psn = 0; psn < MAX_PST; psn++) {
		PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(m->cpu, psn);

		/* Only show non-zero entries */
		if (pst.index_mode != 0 || pst.physical_pfn != 0) {
			const char* mode_str;
			switch (pst.index_mode) {
				case PS_AZI: mode_str = "AZI "; break;
				case PS_ASI: mode_str = "ASI "; break;
				case PS_ADI: mode_str = "ADI "; break;
				default: mode_str = "??? "; break;
			}

			output(ctx, "%4u  %s  0x%04X  0x%08X",
				psn, mode_str, pst.physical_pfn, pst.physical_pfn << PGSHIFT);
			count++;
		}
	}

	if (count == 0) {
		output(ctx, "(no configured entries)");
		output(ctx, "");
		output(ctx, "Use 'mmusetup' to create a demo configuration");
	} else {
		output(ctx, "");
		output(ctx, "Total: %d configured entries (of %d max)", count, MAX_PST);
	}

	return 0;
}

static int cmd_listpcb(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	output(ctx, "=== Configured PCB Domains ===");

	int total_domains = 0;
	int total_segments = 0;

	for (uint32_t domain = 0; domain < MAXDOM; domain++) {
		int domain_has_segments = 0;

		/* Check if this domain has any configured segments */
		for (int seg = 0; seg < 32; seg++) {
			uint16_t pc = nd500_mmu_get_program_capability(m->cpu, domain, seg);
			uint16_t dc = nd500_mmu_get_data_capability(m->cpu, domain, seg);

			if (pc != 0 || dc != 0) {
				if (!domain_has_segments) {
					/* First segment for this domain - print header */
					output(ctx, "");
					output(ctx, "Domain %u:", domain);
					output(ctx, "  Seg  Prog   Data   Description");
					output(ctx, "  ---  ----   ----   -----------");
					domain_has_segments = 1;
					total_domains++;
				}

				/* Build description */
				char desc[80] = "";
				if (pc != 0) {
					uint16_t psn = pc & PC_PSN;
					snprintf(desc, sizeof(desc), "P:PSN=%u", psn);
					if (pc & PC_DIR) strcat(desc, ",DIR");
				}
				if (dc != 0) {
					uint16_t psn = dc & DC_PSN;
					if (desc[0]) strcat(desc, " ");
					char temp[40];
					snprintf(temp, sizeof(temp), "D:PSN=%u", psn);
					strcat(desc, temp);
					if (dc & DC_WRP) strcat(desc, ",WRP");
					if (dc & DC_PAC) strcat(desc, ",PAC");
				}

				output(ctx, "  %3d  %04X   %04X   %s", seg, pc, dc, desc);
				total_segments++;
			}
		}
	}

	if (total_domains == 0) {
		output(ctx, "(no configured domains)");
		output(ctx, "");
		output(ctx, "Use 'mmusetup' to create a demo configuration");
	} else {
		output(ctx, "");
		output(ctx, "Total: %d domains with %d configured segments", total_domains, total_segments);
		output(ctx, "(Maximum: %d domains × 32 segments)", MAXDOM);
	}

	return 0;
}

static int cmd_quit(Nd500Machine* m, CmdContext* ctx, char* args) {
	output(ctx, "quitting...");
	return 1; /* Return 1 to signal quit */
}

/**
 * Execute an initialization script from a file
 * Reads the file line by line and executes each line as a debugger command
 * @param m          Machine instance
 * @param script_path Path to the init script file
 * @return          0 on success, -1 on error (file not found or command failures)
 */
int nd500_execute_init_script(Nd500Machine* m, const char* script_path) {
	if (!m || !script_path) return -1;

	FILE* f = fopen(script_path, "r");
	if (!f) {
		return -1;  /* Script not found - not an error, just skip */
	}

	printf("[init] Executing initialization script: %s\n", script_path);

	/* Set up command context with stdout/stderr callbacks */
	CmdContext ctx;
	ctx.output = NULL;  /* Use default printf behavior */
	ctx.error = NULL;   /* Use default fprintf(stderr) behavior */
	ctx.context = NULL;

	char line[512];
	int line_num = 0;
	int error_count = 0;

	while (fgets(line, sizeof(line), f)) {
		line_num++;

		/* Remove trailing newline */
		size_t len = strlen(line);
		if (len > 0 && line[len - 1] == '\n') {
			line[len - 1] = '\0';
		}

		/* Skip empty lines and comments */
		const char* p = line;
		while (*p && (*p == ' ' || *p == '\t')) p++;  /* Skip leading whitespace */
		if (*p == '\0' || *p == '#') {
			continue;  /* Empty or comment line */
		}

		/* Execute the command */
		printf("[init:%d] %s\n", line_num, line);
		int result = nd500_cmd_execute(m, line, &ctx);
		if (result < 0) {
			fprintf(stderr, "[init:%d] Command failed: %s\n", line_num, line);
			error_count++;
		}
	}

	fclose(f);

	if (error_count > 0) {
		fprintf(stderr, "[init] Script completed with %d errors\n", error_count);
		return -1;
	}

	printf("[init] Script completed successfully\n");
	return 0;
}
