/*
 * ND-500 Debugger Shared Command Library
 * Provides unified command interface for both native CLI and WASM web console
 */

#include "commands.h"
#include "debugger.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"
#include "../ndlib/ndlib.h"
#include "../cpu/cpu_protos.h"
#include "../cpu/nd500_mmu.h"
#include "../cpu/nd500_domain.h"
#include "../cpu/instruction_helpers.h"
#include <ndmon/mon.h>
#include <ndmon/mon_file_table.h>
#include <ndmon/mon_config.h>
#include "nd500_dom.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <time.h>

/* ============================================================================
 * Domain Tracking System
 * Tracks loaded domains for debugger display
 * ============================================================================ */

#define MAX_DOMAINS 256
#define MAX_DOMAIN_NAME 64

/* Information about a loaded domain */
typedef struct {
    int is_loaded;
    uint8_t domain_number;
    char domain_name[MAX_DOMAIN_NAME];
    char filepath[256];
    uint32_t entry_point;
    uint32_t trap_handler;
    int segment_count;
} LoadedDomainInfo;

/* Global domain tracking array */
static LoadedDomainInfo g_loaded_domains[MAX_DOMAINS];

/* Register a loaded domain (called from ndlib_dom_loader.c) */
void nd500_debugger_register_domain(uint8_t domain_num, const char* name,
                                    const char* filepath, uint32_t entry,
                                    uint32_t tha, int seg_count) {
    if (domain_num >= MAX_DOMAINS) return;
    LoadedDomainInfo* info = &g_loaded_domains[domain_num];
    info->is_loaded = 1;
    info->domain_number = domain_num;
    if (name) {
        strncpy(info->domain_name, name, MAX_DOMAIN_NAME - 1);
        info->domain_name[MAX_DOMAIN_NAME - 1] = '\0';
    } else {
        info->domain_name[0] = '\0';
    }
    if (filepath) {
        strncpy(info->filepath, filepath, sizeof(info->filepath) - 1);
        info->filepath[sizeof(info->filepath) - 1] = '\0';
    } else {
        info->filepath[0] = '\0';
    }
    info->entry_point = entry;
    info->trap_handler = tha;
    info->segment_count = seg_count;
}

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
static int cmd_mem_phys(Nd500Machine* m, CmdContext* ctx, char* args);
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
static int cmd_status(Nd500Machine* m, CmdContext* ctx, char* args);
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
static int cmd_dumppt(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_mon(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_quit(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_domverify(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_domain(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_heap(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_stackframe(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_unload(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_showcap(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_showpages(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_memmap(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_trace(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_files(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_file(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_user(Nd500Machine* m, CmdContext* ctx, char* args);
static int cmd_input(Nd500Machine* m, CmdContext* ctx, char* args);

/* Forward declaration for init script execution (defined at end of file) */
int nd500_execute_init_script(Nd500Machine* m, const char* script_path);

/* Command table */
static const CmdEntry g_commands[] = {
	{"help",        cmd_help,         "Show help message"},
	{"?",           cmd_help,         "Show help message"},
	{"m",           cmd_mem,          "Display memory hex dump (data space)"},
	{"mp",          cmd_mem_prog,     "Display memory hex dump (program space)"},
	{"m!",          cmd_mem_phys,     "Display physical memory (bypass MMU)"},
	{"d",           cmd_dis,          "Disassemble instructions"},
	{"dis",         cmd_dis,          "Disassemble instructions"},
	{"disasm",      cmd_dis,          "Disassemble instructions"},
	{"show",        cmd_show,         "Show/toggle debugger options"},
	{"trace",       cmd_trace,        "Set trace output file"},
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
	{"status",      cmd_status,       "Show execution status"},
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
	{"dumppt",      cmd_dumppt,       "Dump page table entries for PSN"},
	{"domverify",   cmd_domverify,    "Verify DOM data in memory matches disk file"},
	{"domain",      cmd_domain,       "Domain management (switch/symbols)"},
	{"heap",        cmd_heap,         "Dump heap variables at TOS"},
	{"stackframe",  cmd_stackframe,   "Dump stack frame at B register"},
	{"sf",          cmd_stackframe,   "Dump stack frame at B register"},
	{"unload",      cmd_unload,       "Unload domain and free resources"},
	{"showcap",     cmd_showcap,      "Show capability tables for a domain"},
	{"showpages",   cmd_showpages,    "Show page mappings for a domain"},
	{"memmap",      cmd_memmap,       "Display memory map (virtual or physical)"},
	{"mon",         cmd_mon,          "MON call settings (log/status/list/info/break)"},
	{"files",       cmd_files,        "List open SINTRAN files"},
	{"file",        cmd_file,         "Show details for open file"},
	{"user",        cmd_user,         "Show/set current SINTRAN user"},
	{"input",       cmd_input,        "Queue console input (e.g., input HELP\\r\\n)"},
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

static const char* g_mon_subcommands[] = {
	"log", "status", "list", "info", "break", NULL
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

/* Log callback wrapper for ndlib_dom_load_to_machine */
static void dom_log_callback(void* ctx, const char* fmt, ...) {
	CmdContext* cmd_ctx = (CmdContext*)ctx;
	if (!cmd_ctx || !cmd_ctx->output) return;
	char buf[1024];
	va_list args;
	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	cmd_ctx->output(buf, cmd_ctx->context);
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
	} else if (strcmp(command, "mon") == 0) {
		return g_mon_subcommands;
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
	output(ctx, "  m [addr [len]]              Hex dump memory - data space (default addr=PC, len=100)");
	output(ctx, "  mp [addr [len]]             Hex dump memory - program space");
	output(ctx, "  m! [addr [len]]             Hex dump physical memory (bypass MMU)");
	output(ctx, "  d [addr [len]]              Disassemble bytes (default addr=PC, len=100)");
	output(ctx, "  show ea [on|off]            Toggle/show effective-address breakdown in disassembly");
	output(ctx, "  show hex [on|off]           Toggle hex bytes in disassembly (default: on)");
	output(ctx, "  show demangle [on|off]      Toggle C-symbol demangling (strip leading _)");
	output(ctx, "  show source [off|asm|c|both] Set source annotations in disassembly");
	output(ctx, "  show trace [on|off]         Toggle instruction execution tracing (console)");
	output(ctx, "  trace file <path> [append]  Write trace to file (overwrites or appends)");
	output(ctx, "  trace off                   Close trace file");
	output(ctx, "  show profile [on|off]      Toggle instruction execution profiling");
	output(ctx, "  show mmu [level]           Set MMU logging (off|errors|trace|all)");
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
	output(ctx, "  dumppt <psn> [start] [cnt]  Dump page table entries for PSN");
	output(ctx, "");
	output(ctx, "SINTRAN MON Call Emulation:");
	output(ctx, "  mon                         Show MON subcommands");
	output(ctx, "  mon log [off|error|warn|info|debug|trace]  Set/show MON logging level");
	output(ctx, "  mon status                  Show MON implementation statistics");
	output(ctx, "  mon list [status]           List MON calls (validated|inprogress|notimpl)");
	output(ctx, "  mon info <number|name>      Show details for specific MON call");
	output(ctx, "  mon break [unimpl|inprog|off]  Break on unimplemented MON calls");
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

/* Physical memory dump - bypasses MMU translation entirely */
static int cmd_mem_phys(Nd500Machine* m, CmdContext* ctx, char* args) {
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

		/* Build hex and ASCII parts - direct physical access, no MMU */
		for (uint32_t j = 0; j < 16; ++j) {
			uint32_t idx = i + j;
			if (idx < len) {
				uint32_t paddr = line_addr + j;
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
	} else if (strcmp(sub, "mmu") == 0) {
		char* val = strtok(NULL, " \t\r\n");
		if (!val) {
			/* No arg: show current status and usage help */
			int cur = nd500_dbg_get_mmu_log_level();
			const char* level_str[] = {"off", "errors", "trace", "all"};
			output(ctx, "MMU logging level: %s", level_str[cur]);
			output(ctx, "");
			output(ctx, "Usage: show mmu [off|errors|trace|all]");
			output(ctx, "  off    - No MMU logging");
			output(ctx, "  errors - Only error messages (default)");
			output(ctx, "  trace  - Translation trace + errors");
			output(ctx, "  all    - All MMU output including successful translations");
			return 0;
		}
		int newv;
		if (strcasecmp(val, "off") == 0) {
			newv = MMU_LOG_OFF;
		} else if (strcasecmp(val, "errors") == 0) {
			newv = MMU_LOG_ERRORS;
		} else if (strcasecmp(val, "trace") == 0) {
			newv = MMU_LOG_TRACE;
		} else if (strcasecmp(val, "all") == 0) {
			newv = MMU_LOG_ALL;
		} else {
			error(ctx, "usage: show mmu [off|errors|trace|all]");
			return -1;
		}
		nd500_dbg_set_mmu_log_level(newv);
		const char* level_str[] = {"off", "errors", "trace", "all"};
		output(ctx, "show mmu: %s", level_str[newv]);
	} else {
		error(ctx, "unknown show option");
		return -1;
	}
	return 0;
}

/*
 * trace command - set trace output file
 *
 * Usage:
 *   trace file <path>          Write trace to file (overwrite)
 *   trace file <path> append   Write trace to file (append)
 *   trace off                  Close trace file
 *   trace                      Show current trace file status
 */
static int cmd_trace(Nd500Machine* m, CmdContext* ctx, char* args) {
	(void)m;
	char* sub = args ? strtok(args, " \t\r\n") : NULL;

	if (!sub) {
		/* Show current status */
		FILE* f = nd500_dbg_get_trace_file();
		if (f && f != stdout) {
			output(ctx, "trace: file output enabled");
		} else if (nd500_dbg_get_trace_mode()) {
			output(ctx, "trace: console output enabled (use 'trace file <path>' for file output)");
		} else {
			output(ctx, "trace: disabled (use 'show trace on' or 'trace file <path>')");
		}
		return 0;
	}

	if (strcasecmp(sub, "off") == 0) {
		nd500_dbg_close_trace_file();
		nd500_dbg_set_trace_mode(0);
		output(ctx, "trace: disabled");
		return 0;
	}

	if (strcasecmp(sub, "file") == 0) {
		char* path = strtok(NULL, " \t\r\n");
		if (!path) {
			error(ctx, "usage: trace file <path> [append]");
			return -1;
		}
		char* mode = strtok(NULL, " \t\r\n");
		int append = (mode && strcasecmp(mode, "append") == 0);

		if (nd500_dbg_set_trace_file_ex(path, append) != 0) {
			error(ctx, "failed to open trace file: %s", path);
			return -1;
		}
		output(ctx, "trace: %s to %s", append ? "appending" : "writing", path);
		return 0;
	}

	error(ctx, "usage: trace file <path> [append] | trace off");
	return -1;
}

static int cmd_step(Nd500Machine* m, CmdContext* ctx, char* args) {
	char* a1 = args ? strtok(args, " \t\r\n") : NULL;
	uint32_t n = nd500_cmd_parse_u32(a1, 1);
	/* Clear any stale traps before stepping - prevents write functions from
	 * silently failing due to traps left over from previous operations
	 * (e.g., disassembly, memory dumps that triggered MMU translations) */
	nd500_dbg_clear_traps();
	/* Set run_flag = 1 for stepping - instructions like BMOVE check run_flag
	 * to detect traps. Without this, run_flag stays at 0 and instructions
	 * incorrectly think a trap occurred after every memory operation. */
	m->run_flag = 1;
	m->stop_reason = STOP_NONE;
	uint32_t executed = 0;
	for (uint32_t i = 0; i < n; ++i) {
		nd500_dbg_step(m, 1);
		executed++;
		if (m->stop_reason != STOP_NONE) break;
	}
	if (m->stop_reason != STOP_NONE) {
		output(ctx, "Stopped: %s at 0x%08X (after %u instruction%s)",
		       nd500_stop_reason_str(m->stop_reason), m->stop_addr,
		       executed, executed == 1 ? "" : "s");
	} else {
		output(ctx, "Stepped %u instruction%s", executed, executed == 1 ? "" : "s");
	}
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

	/* Build flags ASCII string: uppercase=set, lowercase=clear
	 * Format: PDZSCKO (P=Privileged, D=PSD, Z=Zero, S=Sign, C=Carry, K=K-flag, O=Overflow) */
	char flags_ascii[8];
	flags_ascii[0] = (r.ST1 & ND500_FLAG_PIA) ? 'P' : 'p';
	flags_ascii[1] = (r.ST1 & ND500_FLAG_PSD) ? 'D' : 'd';
	flags_ascii[2] = (r.ST1 & ND500_FLAG_Z) ? 'Z' : 'z';
	flags_ascii[3] = (r.ST1 & ND500_FLAG_S) ? 'S' : 's';
	flags_ascii[4] = (r.ST1 & ND500_FLAG_C) ? 'C' : 'c';
	flags_ascii[5] = (r.ST1 & ND500_FLAG_K) ? 'K' : 'k';
	flags_ascii[6] = (r.ST1 & ND500_FLAG_O) ? 'O' : 'o';
	flags_ascii[7] = '\0';

	/* Output format matches C# RetroCore layout */
	output(ctx, "Core Registers:");
	output(ctx, "  PC                   = 0x%08X   - Program Counter - Current instruction address", r.PC);
	output(ctx, "  FLAGS                = 0x%08X   - Status Register Flags", r.FLAGS);
	output(ctx, "  Flags                = %s      - CPU Status Flags as ASCII (uppercase=set, lowercase=clear)", flags_ascii);
	output(ctx, "  ST1                  = 0x%08X   - Status Register (low 32 bits)", r.ST1);
	output(ctx, "  ST2                  = 0x%08X   - Status Register (high 32 bits)", r.ST2);
	output(ctx, "");
	output(ctx, "Integer Registers:");
	output(ctx, "  I1                   = 0x%08X   - Integer register 1 (W1/H1/BY1/BI1)", r.I[0]);
	output(ctx, "  I2                   = 0x%08X   - Integer register 2 (W2/H2/BY2/BI2)", r.I[1]);
	output(ctx, "  I3                   = 0x%08X   - Integer register 3 (W3/H3/BY3/BI3)", r.I[2]);
	output(ctx, "  I4                   = 0x%08X   - Integer register 4 (W4/H4/BY4/BI4)", r.I[3]);
	output(ctx, "");
	output(ctx, "Float Registers:");
	output(ctx, "  A1                   = 0x%08X   - Float accumulator 1 (F1 single, D1 low)", r.A[0]);
	output(ctx, "  A2                   = 0x%08X   - Float accumulator 2 (F2 single, D2 low)", r.A[1]);
	output(ctx, "  A3                   = 0x%08X   - Float accumulator 3 (F3 single, D3 low)", r.A[2]);
	output(ctx, "  A4                   = 0x%08X   - Float accumulator 4 (F4 single, D4 low)", r.A[3]);
	output(ctx, "");
	output(ctx, "FloatExt Registers:");
	output(ctx, "  E1                   = 0x%08X   - Float extension 1 (D1 high 32 bits)", r.E[0]);
	output(ctx, "  E2                   = 0x%08X   - Float extension 2 (D2 high 32 bits)", r.E[1]);
	output(ctx, "  E3                   = 0x%08X   - Float extension 3 (D3 high 32 bits)", r.E[2]);
	output(ctx, "  E4                   = 0x%08X   - Float extension 4 (D4 high 32 bits)", r.E[3]);
	output(ctx, "");
	output(ctx, "Double Registers (64-bit = E:A):");
	/* Convert ND-500 double format to IEEE754 for display */
	uint64_t d1_bits = ((uint64_t)r.E[0] << 32) | r.A[0];
	uint64_t d2_bits = ((uint64_t)r.E[1] << 32) | r.A[1];
	uint64_t d3_bits = ((uint64_t)r.E[2] << 32) | r.A[2];
	uint64_t d4_bits = ((uint64_t)r.E[3] << 32) | r.A[3];
	output(ctx, "  D1                   = %f", nd500_double_to_ieee754(d1_bits));
	output(ctx, "  D2                   = %f", nd500_double_to_ieee754(d2_bits));
	output(ctx, "  D3                   = %f", nd500_double_to_ieee754(d3_bits));
	output(ctx, "  D4                   = %f", nd500_double_to_ieee754(d4_bits));
	output(ctx, "");
	output(ctx, "Addressing Registers:");
	output(ctx, "  P                    = 0x%08X   - Program Counter register", r.PC);
	output(ctx, "  L                    = 0x%08X   - Link register - Return address", r.L);
	output(ctx, "  B                    = 0x%08X   - Base register - Local frame pointer", r.B);
	output(ctx, "  R                    = 0x%08X   - Record register - Structure base pointer", r.R);
	output(ctx, "");
	output(ctx, "Special Registers:");
	output(ctx, "  TOS                  = 0x%08X   - Top of Stack - Stack overflow limit", r.TOS);
	output(ctx, "  LL                   = 0x%08X   - Low Limit - Memory lower bound", r.LL);
	output(ctx, "  HL                   = 0x%08X   - High Limit - Memory upper bound", r.HL);
	output(ctx, "  THA                  = 0x%08X   - Trap Handler Address - Exception entry point", r.THA);
	output(ctx, "  CED                  = 0x%08X   - Current Executing Domain", r.CED);
	output(ctx, "  CAD                  = 0x%08X   - Current Alternative Domain", r.CAD);
	output(ctx, "  PS                   = 0x%08X   - Process Segment register", r.PS);
	output(ctx, "  PSTP                 = 0x%08X   - Physical Segment Table Pointer", r.PSTP);
	output(ctx, "  DITBASE              = 0x%08X   - Domain Information Table base address", r.DITBASE);
	output(ctx, "");
	output(ctx, "Control Registers:");
	output(ctx, "  OTE1                 = 0x%08X   - Own Trap Enable (low 32)", r.OTE1);
	output(ctx, "  OTE2                 = 0x%08X   - Own Trap Enable (high 32)", r.OTE2);
	output(ctx, "  CTE1                 = 0x%08X   - Child Trap Enable (low 32)", r.CTE1);
	output(ctx, "  CTE2                 = 0x%08X   - Child Trap Enable (high 32)", r.CTE2);
	output(ctx, "  MTE1                 = 0x%08X   - Mother Trap Enable (low 32)", r.MTE1);
	output(ctx, "  MTE2                 = 0x%08X   - Mother Trap Enable (high 32)", r.MTE2);
	output(ctx, "  TEMM1                = 0x%08X   - Trap Enable Modification Mask (low 32)", r.TEMM1);
	output(ctx, "  TEMM2                = 0x%08X   - Trap Enable Modification Mask (high 32)", r.TEMM2);

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
		error(ctx, "           OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2 (trap enable/mask)");
		error(ctx, "           PSTP, DITBASE, CED, CAD, PS");
		return -1;
	}

	uint32_t value = nd500_cmd_parse_u32(value_str, 0);

	/* Set register based on name (shared table in debug_api.c) */
	if (nd500_dbg_reg_set_by_name(m->cpu, reg_name, value) == 0) {
		output(ctx, "%s = 0x%08X", reg_name, value);
	} else {
		error(ctx, "unknown register: %s", reg_name);
		error(ctx, "registers: PC, I1-I4, A1-A4, E1-E4, L, B, R, FLAGS, TOS, LL, HL, THA, ST1, ST2");
		error(ctx, "           OTE1, OTE2, CTE1, CTE2, MTE1, MTE2, TEMM1, TEMM2 (trap enable/mask)");
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

	/* Separate I&D de-aliasing for the PSEG/DSEG load path (a.out magic 0411 =
	 * separate instruction & data). The kernel's DATA lives in D-space virtual
	 * [0, a_data+a_bss), NOT contiguous after the text - so a segment-0 DATA read of
	 * D-space virtual V must resolve to the physical address where the DSEG was
	 * actually loaded, i.e. (base_addr + V), while program fetches stay identity
	 * (text at physical == virtual). The segment-0 identity fallback in nd500_mmu.c
	 * offsets data accesses by exactly this data_base. Setting it to 0 (the old value)
	 * made every kernel data read alias into the TEXT region: e.g. slpque[] (bss) read
	 * text bytes instead of its zero-filled image, so wakeup() saw a non-null non-proc
	 * entry and paniced - the NDIX boot panic loop (ND500X_BUG_REPORT.md section 5).
	 * base_addr is where load-dseg placed the DSEG (0x41a94 = a_text for a contiguous
	 * pseg+dseg load). Mirrors the --aout path's g_data_base = a_text. */
	ndlib_aout_set_data_base(base_addr);

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
	if (!m || !m->cpu) {
		error(ctx, "no machine or cpu available");
		return -1;
	}

	/* Parse arguments: loaddom <filepath> [domain] [autostart] [-v|--verbose|/v]
	 * Similar to C#: DOMLOAD <filepath> [domain] [autostart] [-v|--verbose] */
	char* filepath = NULL;
	int target_domain = -1;  /* -1 = auto-allocate */
	int autostart = 1;       /* 1 = switch to domain after load */
	int verbose = 0;         /* 0 = normal, 1 = verbose header dump */

	char* tok = args ? strtok(args, " \t\r\n") : NULL;
	while (tok) {
		/* Check for verbose flag */
		if (strcmp(tok, "-v") == 0 || strcmp(tok, "--verbose") == 0 || strcmp(tok, "/v") == 0) {
			verbose = 1;
		} else if (!filepath) {
			/* First non-flag argument is filepath */
			filepath = tok;
		} else {
			/* Subsequent numeric arguments: domain, then autostart */
			char* endptr;
			long val = strtol(tok, &endptr, 0);
			if (*endptr == '\0') {
				if (target_domain < 0 && val >= 0 && val <= 255) {
					target_domain = (int)val;
				} else if (val <= 1) {
					autostart = (int)val;
				} else {
					error(ctx, "invalid parameter: %s", tok);
					return -1;
				}
			} else {
				error(ctx, "invalid parameter: %s", tok);
				return -1;
			}
		}
		tok = strtok(NULL, " \t\r\n");
	}

	if (!filepath) {
		output(ctx, "Usage: loaddom <filepath> [domain] [autostart] [-v|--verbose]");
		output(ctx, "  filepath  - Path to .dom or .seg file");
		output(ctx, "  domain    - Optional: domain number (0-255), omit to auto-allocate");
		output(ctx, "  autostart - Optional: 0 to not switch, 1 to switch (default)");
		output(ctx, "  -v        - Verbose: show detailed DOM header information");
		output(ctx, "");
		output(ctx, "Examples:");
		output(ctx, "  loaddom prog.dom           - Auto-allocate domain");
		output(ctx, "  loaddom prog.dom 5         - Load into domain 5");
		output(ctx, "  loaddom prog.dom 5 0       - Load into domain 5, don't switch");
		output(ctx, "  loaddom prog.dom -v        - Auto-allocate with verbose output");
		output(ctx, "  loaddom prog.dom 5 -v      - Load into domain 5, verbose");
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
	uint32_t start_addr = nd500_read32(&hdr->raw[0xD8]);

	output(ctx, "Loaded %s: %s", is_dom ? "DOM" : "SEG", filepath);
	output(ctx, "  Linker version: %d.%d", hdr->raw[4], hdr->raw[5]);
	output(ctx, "  Flags: 0x%02X", hdr->raw[6]);
	output(ctx, "  Machine: 0x%02X", hdr->raw[7]);
	output(ctx, "  OS ID: 0x%02X", hdr->raw[8]);
	output(ctx, "  Start addr: 0x%08X", start_addr);
	output(ctx, "  Restart addr: 0x%08X", nd500_read32(&hdr->raw[0xDC]));

	/* Verbose mode: show detailed DOM header information (matching C# DumpHeaderInfo format) */
	if (verbose) {
		/* FILE HEADER Section */
		output(ctx, "");
		output(ctx, "=========================================================");
		output(ctx, "File HEADER: %s", filepath);
		output(ctx, "=========================================================");

		/* LinkLock (octal 0000-0003) */
		uint32_t link_lock = nd500_read32(&hdr->raw[0x00]);
		output(ctx, "LinkLock        : 0x%08X%s", link_lock,
			(link_lock == 0xFFFFFFFF || link_lock == 0x0000FFFF) ? " (UNIVERSAL)" : "");
		output(ctx, "Version         : %d.%d", hdr->raw[4], hdr->raw[5]);

		/* FLAGS (octal 0006) - Domain flags */
		uint8_t flags = hdr->raw[6];
		output(ctx, "FLAGS           : 0x%02X [%s%s%s%s%s]",
			flags,
			(flags & 0x08) ? "TrapBlockValid " : "",
			(flags & 0x10) ? "IsDomainFile " : "",
			(flags & 0x20) ? "IsRootDomain " : "",
			(flags & 0x40) ? "IsSintranIII " : "",
			(flags & 0x80) ? "IsND500 " : "");

		/* MACHINE (octal 0007) - Target machine */
		uint8_t machine = hdr->raw[7];
		const char* machine_name = "Unknown";
		uint8_t target_machine_type = (machine >> 5) & 0x07;
		if (target_machine_type == 0) machine_name = "Norsk Data";
		else if (target_machine_type == 1) machine_name = "Motorola";
		else if (target_machine_type == 2) machine_name = "Intel";
		output(ctx, "MACHINE         : 0x%02X", machine);
		output(ctx, "Machine         : %s", machine_name);

		/* OS ID (octal 0010) */
		uint8_t os_id = hdr->raw[8];
		const char* os_name = "Unknown";
		if (os_id <= 9) os_name = "ND-OS (SINTRAN-III)";
		else if (os_id >= 10 && os_id <= 19) os_name = "UNIX";
		else if (os_id >= 20 && os_id <= 29) os_name = "MS-DOS";
		output(ctx, "OS              : %s [%d]", os_name, os_id);

		/* Subsystem key (octal 012-017, 6 bytes) */
		output(ctx, "Subsystem key   : %02X %02X %02X %02X %02X %02X",
			hdr->raw[10], hdr->raw[11], hdr->raw[12],
			hdr->raw[13], hdr->raw[14], hdr->raw[15]);

		if (is_dom) {
			/* DOM HEADER Section */
			output(ctx, "");
			output(ctx, "=========================================================");
			output(ctx, "DOM HEADER:");
			output(ctx, "=========================================================");

			/* Domain Privileges (octal 0020-0027) */
			uint16_t priv1 = nd500_read16(&hdr->raw[0x10]);
			output(ctx, "DomainPrivileges1  :  0x%04X  [%s%s]",
				priv1,
				(priv1 & 0x8000) ? "EnableEscape " : "",
				(priv1 & 0x4000) ? "PrivilegedInstructions " : "");

			/* COMMON PARTS Section */
			output(ctx, "");
			output(ctx, "=========================================================");
			output(ctx, "COMMON Parts:");
			output(ctx, "=========================================================");

			/* FREIND - Free pointer in name pool (octal 0306-0307) */
			uint16_t freind = nd500_read16(&hdr->raw[0xC6]);
			output(ctx, "FREIND         :  0x%04X", freind);

			/* Debug and Link areas */
			uint32_t deb_lb = nd500_read32(&hdr->raw[0xC8]);
			uint32_t deb_sz = nd500_read32(&hdr->raw[0xCC]);
			uint32_t link_lb = nd500_read32(&hdr->raw[0xD0]);
			uint32_t link_sz = nd500_read32(&hdr->raw[0xD4]);
			output(ctx, "DEBUG LB       :  0x%08X (Lower bound of DEBUG info area within :DOM file)", deb_lb);
			output(ctx, "DEBUG SZ       :  0x%08X (Size of DEBUG info area)", deb_sz);
			output(ctx, "LINK LB        :  0x%08X (Lower bound of LINK info area within :DOM file)", link_lb);
			output(ctx, "LINK SZ        :  0x%08X (Size of LINK info area)", link_sz);

			/* Start and restart addresses */
			output(ctx, "STADDR         :  0x%08X (Start address)", start_addr);
			output(ctx, "RESTADDR       :  0x%08X (Restart address)", nd500_read32(&hdr->raw[0xDC]));

			/* Trap block */
			uint32_t tha   = nd500_read32(&hdr->raw[0xE0]);
			uint32_t mte2  = nd500_read32(&hdr->raw[0xE4]);
			uint32_t mte1  = nd500_read32(&hdr->raw[0xE8]);
			uint32_t ote2  = nd500_read32(&hdr->raw[0xEC]);
			uint32_t ote1  = nd500_read32(&hdr->raw[0xF0]);
			uint32_t cte2  = nd500_read32(&hdr->raw[0xF4]);
			uint32_t cte1  = nd500_read32(&hdr->raw[0xF8]);
			uint32_t temm2 = nd500_read32(&hdr->raw[0xFC]);
			uint32_t temm1 = nd500_read32(&hdr->raw[0x100]);

			output(ctx, "THA            :  0x%08X (traphandler vector address)", tha);
			output(ctx, "MTE2           :  0x%08X", mte2);
			output(ctx, "MTE1           :  0x%08X", mte1);
			output(ctx, "OTE2           :  0x%08X", ote2);
			output(ctx, "OTE1           :  0x%08X", ote1);
			output(ctx, "CTE2           :  0x%08X", cte2);
			output(ctx, "CTE1           :  0x%08X", cte1);
			output(ctx, "TEMM2          :  0x%08X", temm2);
			output(ctx, "TEMM1          :  0x%08X", temm1);

			/* Process priority (octal 0404) */
			uint32_t priority = nd500_read32(&hdr->raw[0x104]);
			output(ctx, "PRIORITY       :  0x%08X (Process priority)", priority);

			/* Source Language mask and MSA Language */
			output(ctx, "");
			output(ctx, "=========================================================");
			output(ctx, " Source Language mask and MSA Language");
			output(ctx, "=========================================================");

			/* LANGUAGE and MSAL (octal 1110) */
			uint32_t lang_msal = nd500_read32(&hdr->raw[0x248]);
			uint8_t msal = lang_msal & 0xFF;
			const char* lang_name = "Unknown";
			switch (msal) {
				case 0: lang_name = "ND-500 Assembler"; break;
				case 1: lang_name = "FORTRAN-500"; break;
				case 2: lang_name = "PLANC"; break;
				case 3: lang_name = "PASCAL"; break;
				case 4: lang_name = "COBOL"; break;
				case 5: lang_name = "BASIC"; break;
				case 6: lang_name = "C"; break;
				case 7: lang_name = "NPL"; break;
				case 8: lang_name = "MAC"; break;
			}
			output(ctx, "LANGUAGE           :  %s", lang_name);
			output(ctx, "LANGUAGE           :  0x%08X", lang_msal);

			/* Free text indexes (octal 1114-1116) */
			uint16_t min_free_text = nd500_read16(&hdr->raw[0x24C]);
			uint16_t max_free_text = nd500_read16(&hdr->raw[0x24E]);
			output(ctx, "MIN                :  0x%04X", min_free_text);
			output(ctx, "MAX                :  0x%04X", max_free_text);
		}

		/* Segments Section */
		output(ctx, "");
		output(ctx, "=========================================================");
		output(ctx, "Segments");
		output(ctx, "=========================================================");

		for (int i = 0; i < (is_dom ? 32 : 1); i++) {
			uint32_t prog_size, prog_addr, data_size, data_addr;
			int is_linked, is_used;
			if (ndlib_dom_get_segment_info(i, &prog_size, &prog_addr, &data_size, &data_addr, &is_linked, &is_used) == 0) {
				if (is_used || prog_size > 0 || data_size > 0) {
					output(ctx, "Segment      : %d", i);
					output(ctx, "-- PROGRAM --");
					output(ctx, "LB           : 0x%08X (Lower bound of PROGRAM segment)", prog_addr);
					output(ctx, "SZ           : 0x%08X (Size of PROGRAM segment)", prog_size);
					output(ctx, "-- DATA --");
					output(ctx, "LB           : 0x%08X (Lower bound of DATA segment)", data_addr);
					output(ctx, "SZ           : 0x%08X (Size of DATA segment)", data_size);
					output(ctx, "----");
				}
			}
		}

		output(ctx, "=========================================================");
	}

	/* Load segments into internal buffers */
	rc = ndlib_load_dom_segments();
	if (rc != 0) {
		error(ctx, "warning: failed to load segments");
	}

	/* Load segments to memory, configure MMU and domain system */
	output(ctx, "");
	output(ctx, "Physical Memory Layout:");

	int loaded_domain = -1;
	rc = ndlib_dom_load_to_machine(m, m->cpu, target_domain, dom_log_callback, ctx, NULL, &loaded_domain);
	if (rc != 0) {
		error(ctx, "failed to configure machine for DOM execution");
		return -1;
	}

	/* Display domain loaded summary */
	output(ctx, "");
	output(ctx, "============================================================");
	output(ctx, "  Domain Loaded");
	output(ctx, "============================================================");
	output(ctx, "  Domain:     %d%s", loaded_domain, autostart ? " (active)" : "");
	output(ctx, "  Entry:      0x%08X", start_addr);
	output(ctx, "");
	if (!autostart) {
		output(ctx, "  Domain loaded but NOT started (autostart=0)");
	} else {
		output(ctx, "  Use 'run' to start execution");
	}
	output(ctx, "============================================================");

	return 0;
}

static int cmd_run(Nd500Machine* m, CmdContext* ctx, char* args) {
	nd500_dbg_run(m);
	output(ctx, "running...");
	return 0;
}

static int cmd_stop(Nd500Machine* m, CmdContext* ctx, char* args) {
	(void)args;
	nd500_dbg_stop(m);
	if (m->stop_reason != STOP_NONE) {
		output(ctx, "Stopped: %s at 0x%08X",
		       nd500_stop_reason_str(m->stop_reason), m->stop_addr);
	} else {
		m->stop_reason = STOP_USER_REQUESTED;
		m->stop_addr = m->cpu ? m->cpu->PC : 0;
		output(ctx, "Stopped at PC=0x%08X", m->stop_addr);
	}
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

static int cmd_status(Nd500Machine* m, CmdContext* ctx, char* args) {
	(void)args;
	if (m->run_flag) {
		output(ctx, "Status: running at PC=0x%08X", m->cpu ? m->cpu->PC : 0);
	} else {
		if (m->stop_reason != STOP_NONE) {
			output(ctx, "Status: stopped - %s at 0x%08X",
			       nd500_stop_reason_str(m->stop_reason), m->stop_addr);
		} else {
			output(ctx, "Status: stopped at PC=0x%08X", m->cpu ? m->cpu->PC : 0);
		}
	}
	return 0;
}

static int cmd_symb(Nd500Machine* m, CmdContext* ctx, char* args) {
	/* TODO: Implement callback-based symbol listing */
	ndlib_symbols_list_all();
	return 0;
}

/* Helper: Calculate segment size from PST entry */
static uint32_t calculate_segment_size(Nd500Machine* m, Nd500Cpu* cpu, uint16_t capability) {
	if (!m || !cpu) return 0;
	
	/* Extract PSN from capability */
	uint16_t psn = capability & PC_PSN;
	if (psn == 0) return 0;
	
	/* Get PST entry */
	PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(cpu, psn);
	
	/* Check if capability is direct (not indirect) */
	/* PC_DIR is 0x0000, PC_IND is 0x8000 - direct means PC_IND bit is clear */
	if ((capability & PC_IND) == 0) {
		/* Direct mapping: 1 page (2KB) */
		return NBPG;
	}
	
	/* Check PST index mode */
	if (pst.index_mode == PS_AZI) {
		/* Direct addressed page: 1 page (2KB) */
		return NBPG;
	} else if (pst.index_mode == PS_ASI) {
		/* Single-level paging: count valid PTEs in page table */
		uint32_t page_table_base = pst.physical_pfn << PGSHIFT;
		uint32_t page_count = 0;
		
		/* Scan page table entries (up to 512 entries per page table) */
		for (uint32_t i = 0; i < NPTEPG; i++) {
			uint32_t pte_addr = page_table_base + (i * 4);
			uint32_t pte_value = nd500_bus_read32(m, pte_addr);
			
			/* Extract PFN from PTE (bits 31:2) */
			uint32_t pte_pfn = (pte_value >> 2) & 0x3FFFFFFF;
			
			/* Valid if PFN != 0 */
			if (pte_pfn != 0) {
				page_count++;
			} else {
				/* Stop at first zero entry (page tables are contiguous) */
				break;
			}
		}
		
		return page_count * NBPG;
	} else if (pst.index_mode == PS_ADI) {
		/* Two-level paging: count valid L1 PTEs, then count L2 PTEs */
		uint32_t l1_table_base = pst.physical_pfn << PGSHIFT;
		uint32_t total_pages = 0;
		
		/* Scan L1 page table entries (up to 128 entries) */
		for (uint32_t l1_idx = 0; l1_idx < 128; l1_idx++) {
			uint32_t l1_pte_addr = l1_table_base + (l1_idx * 4);
			uint32_t l1_pte_value = nd500_bus_read32(m, l1_pte_addr);
			uint32_t l1_pte_pfn = (l1_pte_value >> 2) & 0x3FFFFFFF;
			
			if (l1_pte_pfn == 0) break; /* Stop at first zero L1 entry */
			
			/* Scan L2 page table */
			uint32_t l2_table_base = l1_pte_pfn << PGSHIFT;
			for (uint32_t l2_idx = 0; l2_idx < NPTEPG; l2_idx++) {
				uint32_t l2_pte_addr = l2_table_base + (l2_idx * 4);
				uint32_t l2_pte_value = nd500_bus_read32(m, l2_pte_addr);
				uint32_t l2_pte_pfn = (l2_pte_value >> 2) & 0x3FFFFFFF;
				
				if (l2_pte_pfn != 0) {
					total_pages++;
				} else {
					break; /* Stop at first zero L2 entry */
				}
			}
		}
		
		return total_pages * NBPG;
	}
	
	return 0;
}

static int cmd_segments(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}
	
	/* Segments only exist when MMU is enabled (segments are part of virtual address space) */
	if (!nd500_machine_mmu_is_enabled(m)) {
		error(ctx, "MMU is disabled - segments only exist when MMU is enabled");
		error(ctx, "When MMU is disabled, addresses are physical (no virtual segments)");
		return -1;
	}
	
	/* MMU enabled: Get segment info from PCB capabilities */
	uint8_t domain = m->cpu->CED; /* Current executing domain */
	
	uint32_t text_base = 0, text_size = 0;
	uint32_t data_base = 0, data_size = 0;
	uint32_t bss_base = 0, bss_size = 0;
	
	/* Segment 0: DATA (virtual address 0x00000000) */
	uint16_t data_cap = nd500_mmu_get_data_capability(m->cpu, domain, 0);
	if (data_cap != 0) {
		data_base = 0x00000000; /* Segment 0 */
		data_size = calculate_segment_size(m, m->cpu, data_cap);
	}
	
	/* Segment 1: PROG/TEXT (virtual address 0x08000000) */
	uint16_t prog_cap = nd500_mmu_get_program_capability(m->cpu, domain, 1);
	if (prog_cap != 0) {
		text_base = 0x08000000; /* Segment 1 */
		text_size = calculate_segment_size(m, m->cpu, prog_cap);
	}
	
	/* BSS is typically part of DATA segment, but we don't have separate tracking */
	/* For now, set BSS to 0 */
	bss_base = 0;
	bss_size = 0;

	output(ctx, "=== SEGMENT LAYOUT (Virtual Address Space) ===");
	output(ctx, "Domain: %u (CED)", domain);
	output(ctx, "");
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
		int reg_index = wp_register_index_for_name(a2);
		if (reg_index < 0) {
			error(ctx, "Unknown register: %s", a2);
			return -1;
		}
		int wid = wp_add_register(m->bp_mgr, a2, (uint32_t)reg_index);
		if (wid >= 0 && m->cpu) {
			/* Prime with the current value so the watch fires on the
			 * next change, not immediately */
			uint32_t cur[WP_REG_INDEX_COUNT] = {
				m->cpu->PC, m->cpu->I[0], m->cpu->I[1], m->cpu->I[2],
				m->cpu->I[3], m->cpu->L, m->cpu->B, m->cpu->R
			};
			m->bp_mgr->watchpoints[wid].last_value = cur[reg_index];
		}
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

				/* PTE hardware format (pte.h): pg_prot@31, pg_pfnum@[29:0] */
				uint8_t protection = (has_write) ? 0 : 1;  /* 0=writable, 1=read-only */
				uint32_t pte_value = ((uint32_t)protection << 31) | (page_num & 0x3FFFFFFF);

				/* Write PTE to physical memory (big-endian via bus_write32) */
				nd500_bus_write32(m, pte_addr, pte_value);
			}

			/* Create PST entry pointing to page table (PS_ASI mode) */
			uint32_t page_table_pfn = page_table_addr >> PGSHIFT;
			nd500_mmu_set_pst_entry(m->cpu, next_psn, PS_ASI, page_table_pfn);

			output(ctx, "  Segment %u: PST[%u] -> page table at 0x%08X (%u pages)",
				seg, next_psn, page_table_addr, seg_num_pages);

			/* Set capabilities for this segment */
			if (has_exec) {
				uint16_t pc = next_psn;
				nd500_mmu_set_program_capability(m->cpu, 0, seg, pc);
				output(ctx, "    Prog capability: PSN %u", next_psn);
			}

			if (has_read || has_write) {
				uint16_t dc = next_psn;
				if (has_write) {  /* DC_WRP = Write Permitted */
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

				/* PTE hardware format (pte.h): pg_prot@31, pg_pfnum@[29:0] */
				uint8_t protection = (has_write) ? 0 : 1;  /* 0=writable, 1=read-only */
				uint32_t pte_value = ((uint32_t)protection << 31) | (phys_page_num & 0x3FFFFFFF);

				/* Write PTE to physical memory (big-endian via bus_write32) */
				nd500_bus_write32(m, pte_addr, pte_value);
			}

			/* Create PST entry pointing to page table (PS_ASI mode) */
			uint32_t page_table_pfn = page_table_addr >> PGSHIFT;
			nd500_mmu_set_pst_entry(m->cpu, next_psn, PS_ASI, page_table_pfn);

			output(ctx, "  Segment %u: PST[%u] → page table at 0x%08X (%u pages)",
				seg, next_psn, page_table_addr, seg_num_pages);

			/* Set capabilities for this segment */
			if (has_exec) {
				uint16_t pc = next_psn;
				nd500_mmu_set_program_capability(m->cpu, 0, seg, pc);
				output(ctx, "    Prog capability: PSN %u", next_psn);
			}

			if (has_read || has_write) {
				uint16_t dc = next_psn;
				if (has_write) {  /* DC_WRP = Write Permitted */
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

	} else if (strcmp(subcmd, "acronyms") == 0 || strcmp(subcmd, "help") == 0) {
		output(ctx, "=== MMU Acronyms and Abbreviations ===");
		output(ctx, "");
		output(ctx, "Address Translation:");
		output(ctx, "  PSN     Physical Segment Number (index into PST)");
		output(ctx, "  PFN     Page Frame Number (physical memory page)");
		output(ctx, "  PST     Physical Segment Table (maps PSN to physical pages)");
		output(ctx, "  PCB     Process Control Block (per-process capabilities)");
		output(ctx, "  PTE     Page Table Entry (individual page mapping)");
		output(ctx, "");
		output(ctx, "Capability Flags (16-bit capability word):");
		output(ctx, "  PC_IND  0x8000  Program Capability Indirect (use descriptor)");
		output(ctx, "  DC_WRP  0x0080  Data Capability Write Permitted");
		output(ctx, "  DC_IND  0x8000  Data Capability Indirect");
		output(ctx, "");
		output(ctx, "PST Entry Flags:");
		output(ctx, "  PS_ASI  0x80    Address Space Identifier present");
		output(ctx, "  PS_WRP  0x40    Write Protect");
		output(ctx, "  PS_REF  0x20    Referenced");
		output(ctx, "  PS_MOD  0x10    Modified");
		output(ctx, "");
		output(ctx, "MMU Control Instructions:");
		output(ctx, "  PMON    Program MMU ON");
		output(ctx, "  PMOF    Program MMU OFF");
		output(ctx, "  DMON    Data MMU ON");
		output(ctx, "  DMOF    Data MMU OFF");
		output(ctx, "");
		output(ctx, "Registers:");
		output(ctx, "  PSTP    Physical Segment Table Pointer");
		output(ctx, "  DITBASE Domain Information Table Base");
		output(ctx, "  CED     Current Executing Domain");
		output(ctx, "  CAD     Current Alternative Domain");
		output(ctx, "  PS      Process Segment");

	} else {
		error(ctx, "usage: mmu [on|off|enable|disable|identity|map|acronyms] ...");
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

	if (!prog_enabled && !data_enabled) {
		output(ctx, "");
		output(ctx, "NOTE: MMU is disabled - PST and PCB entries exist but are not active");
		output(ctx, "      Address translation is not performed when MMU is disabled");
	}

	output(ctx, "");
	output(ctx, "Virtual Address Format (ND-05.009.4 Reference Manual, p53-54):");
	output(ctx, "  [31-27] Segment  (5 bits)  - 32 segments max");
	output(ctx, "  [26-20] L1 Index (7 bits)  - 128 L1 entries (PS_ADI only)");
	output(ctx, "  [19-11] L2 Index (9 bits)  - 512 L2 entries (PS_ASI/PS_ADI)");
	output(ctx, "  [10-0]  Offset   (11 bits) - 2048 bytes per page");
	output(ctx, "");
	output(ctx, "PST Index Modes:");
	output(ctx, "  PS_AZI (0): Direct - 1 page max (2KB)");
	output(ctx, "  PS_ASI (1): Single-level - 512 pages max (1MB)");
	output(ctx, "  PS_ADI (2): Two-level - 65536 pages max (128MB)");
	output(ctx, "");
	output(ctx, "Use 'listpst' to see all configured PST entries");
	output(ctx, "Use 'listpcb' to see all configured domains and segments");
	output(ctx, "Use 'dumppt <psn>' to view page table entries");

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
	if (!nd500_machine_mmu_is_enabled(m)) {
		output(ctx, "NOTE: MMU is disabled - this entry exists but is not active");
		output(ctx, "");
	}
	output(ctx, "Index Mode:    %u (%s)", pst.index_mode,
		pst.index_mode == PS_AZI ? "PS_AZI - Direct" :
		pst.index_mode == PS_ASI ? "PS_ASI - Single-level paging" :
		pst.index_mode == PS_ADI ? "PS_ADI - Two-level paging" : "Unknown");
	output(ctx, "Physical PFN:  0x%04X (Physical address: 0x%08X)",
		pst.physical_pfn, pst.physical_pfn << PGSHIFT);

	if (pst.index_mode == PS_AZI) {
		output(ctx, "");
		output(ctx, "Direct Mapping (PS_AZI):");
		output(ctx, "  Physical frame: 0x%04X -> 0x%08X", pst.physical_pfn, pst.physical_pfn << PGSHIFT);
		output(ctx, "  Max size: 1 page (2KB)");
		output(ctx, "  Requires: L1=0, L2=0 in virtual address");
	} else if (pst.index_mode == PS_ASI) {
		output(ctx, "");
		output(ctx, "Single-Level Paging (PS_ASI):");
		output(ctx, "  Page table at: 0x%08X", pst.physical_pfn << PGSHIFT);
		output(ctx, "  Max size: 512 pages (1MB)");
		output(ctx, "  L2 index range: 0-511 (9 bits)");
		output(ctx, "  Requires: L1=0 in virtual address");
		output(ctx, "  Use 'dumppt %u' to view page table entries", psn);
	} else if (pst.index_mode == PS_ADI) {
		output(ctx, "");
		output(ctx, "Two-Level Paging (PS_ADI):");
		output(ctx, "  L1 page table at: 0x%08X", pst.physical_pfn << PGSHIFT);
		output(ctx, "  Max size: 65536 pages (128MB)");
		output(ctx, "  L1 index range: 0-127 (7 bits)");
		output(ctx, "  L2 index range: 0-511 (9 bits)");
		output(ctx, "  Use 'dumppt %u' to view L1 page table entries", psn);
	}

	return 0;
}

static int cmd_showpcb(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	/* PCB capabilities only exist when MMU is enabled */
	if (!nd500_machine_mmu_is_enabled(m)) {
		error(ctx, "MMU is disabled - PCB capabilities only exist when MMU is enabled");
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
		if (pc != 0) {
			output(ctx, "  PSN:     %u (0x%03X)", pc & PC_PSN, pc & PC_PSN);
			/* PC_DIR is 0x0000, PC_IND is 0x8000 - check PC_IND bit instead */
			int is_indirect = (pc & PC_IND) != 0;
			output(ctx, "  Type:    %s", is_indirect ? "Indirect (PC_IND set)" : "Direct (PC_IND clear)");
			if (is_indirect) {
				uint32_t target_domain = (pc & PC_DOM) >> 5;
				uint32_t target_segment = pc & PC_SEG;
				output(ctx, "  Target:  Domain %u, Segment %u", target_domain, target_segment);
			}
			if (pc & PC_OMC) {
				output(ctx, "  OMC:     Other Machine Call");
			}
		} else {
			output(ctx, "  (not configured)");
		}
		output(ctx, "");
		output(ctx, "Data Capability:    0x%04X", dc);
		if (dc != 0) {
			output(ctx, "  PSN:     %u (0x%03X)", dc & DC_PSN, dc & DC_PSN);
			output(ctx, "  WRP:     %u (%s)", (dc & DC_WRP) ? 1 : 0, (dc & DC_WRP) ? "Write permitted" : "Read-only");
			output(ctx, "  PAC:     %u (%s)", (dc & DC_PAC) ? 1 : 0, (dc & DC_PAC) ? "User accessible" : "Kernel only");
			if (dc & DC_SHS) {
				output(ctx, "  SHS:     Shared segment (cache disabled)");
			}
		} else {
			output(ctx, "  (not configured)");
		}
	} else {
		/* Show all segments for domain */
		output(ctx, "=== PCB Domain %u ===", domain);
		output(ctx, "Seg  Prog Cap  Data Cap");
		output(ctx, "---  --------  --------");

		int found_any = 0;
		for (int seg = 0; seg < 32; seg++) {
			uint16_t pc = nd500_mmu_get_program_capability(m->cpu, domain, seg);
			uint16_t dc = nd500_mmu_get_data_capability(m->cpu, domain, seg);

			/* Only show non-zero entries */
			if (pc != 0 || dc != 0) {
				output(ctx, "%3d  %04X      %04X", seg, pc, dc);
				found_any = 1;
			}
		}
		
		if (!found_any) {
			output(ctx, "(no configured segments)");
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

	/* Extract address components per ND-500 architecture */
	int segment  = (vaddr >> SGSHIFT) & 0x1F;
	int l1_index = (vaddr >> L1_INDEX_SHIFT) & L1_INDEX_MASK;
	int l2_index = (vaddr >> L2_INDEX_SHIFT) & L2_INDEX_MASK;
	int offset   = vaddr & (NBPG - 1);

	output(ctx, "=== Virtual Address Translation ===");
	output(ctx, "Virtual Address: 0x%08X", vaddr);
	output(ctx, "  Segment:  %d (0x%02X)", segment, segment);
	output(ctx, "  L1 Index: %d (0x%02X)  [for PS_ADI]", l1_index, l1_index);
	output(ctx, "  L2 Index: %d (0x%03X)  [for PS_ASI/PS_ADI]", l2_index, l2_index);
	output(ctx, "  Offset:   %d (0x%03X)", offset, offset);
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

	/* Detect translation failure:
	 * - paddr == 0 when vaddr != 0 is an obvious failure
	 * - paddr == vaddr when MMU is enabled strongly suggests failure (trap was raised)
	 *   because with separate I/D spaces, virtual shouldn't equal physical
	 */
	int translation_failed = (paddr == 0 && vaddr != 0) ||
	                         (paddr == vaddr && nd500_machine_mmu_is_enabled(m));

	if (translation_failed) {
		output(ctx, "Translation FAILED (trap raised)");
		output(ctx, "  Returned address: 0x%08X (virtual address unchanged)", paddr);
		output(ctx, "  Possible causes:");
		output(ctx, "  - Invalid capability (null)");
		output(ctx, "  - Protection violation (write to read-only)");
		output(ctx, "  - Page fault (page not present)");
		output(ctx, "  - Invalid PSN in capability");
	} else {
		output(ctx, "Physical Address: 0x%08X", paddr);
		output(ctx, "  PFN:    0x%04X", paddr >> PGSHIFT);
		output(ctx, "  Offset: 0x%03X", paddr & (NBPG - 1));
	}

	return 0;
}

/* Write a big-endian halfword to an ND-500 virtual address through the DATA
 * MMU, mirroring mon_write_halfword_cb (src/cpu/nd500_indirect.c). Passing
 * is_write=1 makes a cap-0 segment demand-allocate exactly as a kernel data
 * write would, so segment 6 gets backed and the bytes are visible to BOTH the
 * kernel and the MON handlers (which translate through the same MMU). */
static void sintran_write_halfword(Nd500Cpu* cpu, uint32_t vaddr, uint16_t val) {
	uint32_t phys = vaddr;
	if (cpu->machine && cpu->machine->mmu_enabled)
		phys = nd500_mmu_translate(cpu, vaddr, 1, 0);  /* is_write, data */
	nd500_bus_write8(cpu->machine, phys,     (uint8_t)(val >> 8));  /* BE hi byte */
	nd500_bus_write8(cpu->machine, phys + 1, (uint8_t)val);         /* BE lo byte */
}

/* SINTRAN shared-memory init: Xmsg ring-buffer descriptors (segment 6).
 *
 * On real hardware SINTRAN (the ND-100 side) sets up the ND-100<->ND-500
 * shared segment before the NDIX kernel runs. The kernel's R_init()
 * (if/xg.c:399) REQUIRES the two ring-buffer headers to be pre-initialized and
 * panics ("Xmsg command/response buffer not initialized") otherwise:
 *
 *   xmsg_cmd_buf  @ 0x30000000 : p=0, k=0, mp=NXMSGCMD  (102)
 *   xmsg_resp_buf @ 0x30000800 : p=0, k=0, mp=NXMSGRESP (113)
 *
 * p (offset 0) and k (offset 2) are already 0 because segment 6 is
 * demand-allocated zeroed, so only the mp field (offset 4, a big-endian short)
 * needs writing. Addresses AND values were verified by disassembling _R_init at
 * 0x3EF42: "h comp2 $0x30000004,#102" and "h comp2 $0x30000804,#113". Note the
 * response struct is UNPADDED on the ND-500 compiler, so
 * NXMSGRESP=(0x800-6)/sizeof(xmsg_resp=18)=113 (NOT 102 - the command struct is
 * 20 bytes -> 102). These match the RetroCore NDSharedMemory reference
 * (XMSG_CMD_BUFFER=0x30000000, XMSG_RESP_BUFFER=0x30000800). */
static void sintran_init_xmsg_ringbuffers(Nd500Cpu* cpu) {
	sintran_write_halfword(cpu, 0x30000004u, 102);  /* xmsg_cmd_buf.mp  = NXMSGCMD  */
	sintran_write_halfword(cpu, 0x30000804u, 113);  /* xmsg_resp_buf.mp = NXMSGRESP */
}

/* Write a big-endian 32-bit word to an ND-500 virtual address through the DATA
 * MMU (same demand-alloc path as sintran_write_halfword). */
static void sintran_write_word(Nd500Cpu* cpu, uint32_t vaddr, uint32_t val) {
	sintran_write_halfword(cpu, vaddr,     (uint16_t)(val >> 16));
	sintran_write_halfword(cpu, vaddr + 2, (uint16_t)val);
}

/* SINTRAN shared-memory init: the IPL (Interrupt Priority Level) record,
 * struct ipl_rec, at the fixed shared-segment address _iplrec = 0x30001000
 * (locore.c:116). Layout (icb.h:31, offsets in bytes):
 *   ip_next   @0 (long)  : outstanding-interrupt descriptor, ND-100 word addr;
 *                          -1 (0xFFFFFFFF) means "none pending"
 *   ip_current@4 (short) : current IPL
 *   ip_mask   @6 (short) : IPL mask
 *   ip_lock   @8 (short) : spinlock byte
 *
 * On real hardware SINTRAN owns this record and queues interrupt descriptors
 * into ip_next; when idle it holds -1. The NDIX kernel never initializes it
 * (machdep.c:834 only does `iplp = &iplrec`); _splx and _intvec (locore.c:1413,
 * 791) only READ ip_next, short-circuiting on -1. Segment 6 is demand-allocated
 * ZEROED, so ip_next=0, which _splx treats as a real descriptor pointer:
 *   r3 = ip_next<<1 - shseg + sharebase = 0 - 0x30000800 + 0x30000000 = -0x800
 *   deref [r3+4] = 0xFFFFF804  -> PS_AZI page fault (verified: exact fault addr).
 * ip_current/ip_mask/ip_lock are correctly 0 from the demand-zero, so only
 * ip_next needs the -1 sentinel. (shseg = htob(sharedseg)+NBPG = 0x30000800.) */
static void sintran_init_iplrec(Nd500Cpu* cpu) {
	sintran_write_word(cpu, 0x30001000u, 0xFFFFFFFFu);  /* iplrec.ip_next = -1 (none) */
}

/* Map domain-0 logical segment `seg` to a contiguous physical region
 * [phys_base, phys_base + npages*2048) via a PS_ASI page table at pt_phys, using
 * PST index `psn`, as a writable data segment. Used to make _Pst/_pcbtab reach
 * the physical PST/DIT the emulator MMU reads. Page-table entries and the PST
 * entry are written in the hardware pte.h format (pg_pfnum@[29:0]). */
static void sintran_map_segment_to_phys(Nd500Machine* m, int seg, uint32_t phys_base,
                                        uint32_t npages, uint32_t psn, uint32_t pt_phys) {
	for (uint32_t i = 0; i < npages; i++) {
		uint32_t pfn = (phys_base >> PGSHIFT) + i;      /* prot 0 = read/write */
		uint32_t pte = pfn & 0x3FFFFFFFu;
		uint32_t a = pt_phys + i * 4u;
		nd500_bus_write8(m, a,   (uint8_t)(pte >> 24));
		nd500_bus_write8(m, a+1, (uint8_t)(pte >> 16));
		nd500_bus_write8(m, a+2, (uint8_t)(pte >> 8));
		nd500_bus_write8(m, a+3, (uint8_t)pte);
	}
	nd500_mmu_set_pst_entry(m->cpu, (int)psn, PS_ASI, pt_phys >> PGSHIFT);
	nd500_mmu_set_data_capability(m->cpu, 0, seg, (uint16_t)(psn | DC_WRP));
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

	/* Set the guest MMU-table base registers BEFORE any capability/PST setup so
	 * the set_* mirroring lands in the right physical tables. The tables live in
	 * the free gap between the kernel image+bss (ends ~0x80000) and kernel free
	 * memory (firstaddr, phys 0x100000): PST at 0x80000 (32KB), DIT at 0x90000
	 * (64KB), seg 27/28 page tables at 0xA0000. translate() reads these. */
	m->cpu->PSTP    = 0x00080000;
	m->cpu->DITBASE = 0x00090000;
	/* Zero the PST (32KB) and DIT (64KB) so unset segments read capability 0
	 * (=> demand-map / identity fallback) instead of stale RAM garbage. */
	for (uint32_t a = 0x00080000; a < 0x000A0000; a++)
		nd500_bus_write8(m, a, 0);

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
		/* PC_DIR is 0x0000 - direct is absence of PC_IND flag, not a flag to set */
		nd500_mmu_set_program_capability(m->cpu, 0, seg, seg);
	}
	output(ctx, "  Prog segments [0-127]   → PSN [0-127]   (virtual 0x00000000-0x3F800000)");

	/* Data segments 0-127: Each segment i maps to PSN 128+i (phys 0x00040000+).
	 * Kernel data is READ/WRITE, so grant DC_WRP - without it every kernel data
	 * store (including the stack-frame [B+8] SP write the NDIX kernel does in
	 * INIT/ENTS) hits "WRITE DENIED! missing DC_WRP flag".
	 *
	 * Two segments must NOT be pre-mapped with the demo's single 2KB PS_AZI page,
	 * because their real extents exceed 2KB (an access past offset 0x7FF faults
	 * with "PS_AZI page fault L2!=0"):
	 *   - segment 0  = the flat-loaded kernel image (text+data+const, virtual
	 *     0x0..< physRAM). Leaving its data capability 0 lets the identity
	 *     fallback back it (virtual == physical, writes allowed) - exactly what
	 *     the PROGRAM side already does (program cap 0 -> identity), so kernel
	 *     globals/consts above 2KB (e.g. vaddr 0x00022924) resolve.
	 *   - segment 29 = the u-area / kernel stack (virtual 0xE8000000, 8KB, beyond
	 *     physRAM). Leaving its capability 0 lets the segment-demand allocator
	 *     back it as a writable, paged PS_ADI segment big enough for the stack. */
	for (uint32_t seg = 0; seg < 128; seg++) {
		/* Leave two segments capability 0 so the correct fallback backs them
		 * instead of the demo's too-small 2KB PS_AZI page:
		 *   seg 0  = flat-loaded kernel image (text+data+const, virtual 0x0 ..
		 *            < physRAM) -> identity fallback (virtual == physical, writes
		 *            allowed) - exactly what the PROGRAM side already does, so
		 *            kernel globals/consts above 2KB (e.g. vaddr 0x00022924) resolve.
		 *   seg 29 = the u-area / kernel stack (virtual 0xE8000000, 8KB, beyond
		 *            physRAM) -> segment-demand allocator backs it writable + paged
		 *            (PS_ADI) big enough for the whole stack. Fixes the reported
		 *            [B+8] SP write dropping and the RET PREVB=0 stack underflow.
		 * The OTHER data segments keep the demo mapping: they carry loaded DSEG
		 * data the kernel reads early, so we must NOT replace them with zeroed
		 * demand pages - just make them writable (DC_WRP). */
		if (seg == 0 || (seg >= 1 && seg <= 30)) {
			continue;  /* seg 0 = identity image; 1..30 = runtime kernel tables demand-backed PS_ADI
			            * (the demo 2KB PS_AZI page is too small for the kernel's real segments). */
		}
		nd500_mmu_set_data_capability(m->cpu, 0, seg, (128 + seg) | DC_WRP);
	}
	output(ctx, "  Data segments   → RW (seg 0 = identity image, seg 29 = demand-backed u-area)");

	/* Special: Segment 31 for Domain 0 = ND-100 Other Machine (INDIRECT + OMC) */
	/* Bit 15 = 1 (INDIRECT), Bit 14 = 1 (OMC), Domain=0, Segment=1 */
	nd500_mmu_set_program_capability(m->cpu, 0, 31, PC_IND | PC_OMC | (0 << 5) | 1);
	output(ctx, "  Prog segment 31         → INDIRECT OMC Domain=0 Seg=1 (ND-100)");

	output(ctx, "");
	output(ctx, "Domain 1 (User1):");
	/* Code segments 0-127: Each segment i maps to PSN 256+i (phys 0x00080000+) */
	for (uint32_t seg = 0; seg < 128; seg++) {
		/* PC_DIR is 0x0000 - direct is absence of PC_IND flag */
		nd500_mmu_set_program_capability(m->cpu, 1, seg, (256 + seg));
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
		/* PC_DIR is 0x0000 - direct is absence of PC_IND flag */
		nd500_mmu_set_program_capability(m->cpu, 2, seg, (512 + seg));
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

	/* Map the kernel's own table-access segments so _Pst (seg 27, 0xd8000000)
	 * reaches physical PSTP and _pcbtab (seg 28, 0xe0000000) reaches physical
	 * DITBASE. Without this the kernel's writes to kern_dcap/Pst (kpcbinit,
	 * __resume, newproc) land in demand pages the MMU never reads. PS_ASI page
	 * tables at 0xA0000 / 0xA1000; high PSNs to avoid the demo's 0-767. */
	sintran_map_segment_to_phys(m, 27, m->cpu->PSTP,    16, 800, 0x000A0000); /* _Pst */
	sintran_map_segment_to_phys(m, 28, m->cpu->DITBASE, 32, 801, 0x000A1000); /* _pcbtab */
	output(ctx, "Mapped seg 27 -> PSTP (0x80000), seg 28 -> DITBASE (0x90000)");

	output(ctx, "");
	output(ctx, "=== MMU Registers ===");
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

	/* SINTRAN's job on context load: initialize the Xmsg ring-buffer
	 * descriptors in the shared segment so the NDIX kernel's R_init() does
	 * not panic. Done after MMU enable so the write translates through the
	 * data MMU and demand-backs segment 6. See helper above for the verified
	 * addresses/values (_R_init disasm at 0x3EF42). */
	sintran_init_xmsg_ringbuffers(m->cpu);
	output(ctx, "Xmsg ring buffers initialized (cmd.mp=102, resp.mp=113 @ seg 6)");

	/* Also SINTRAN's job: seed the IPL record's ip_next to -1 ("no interrupt
	 * pending"). Without it _splx derefs a zeroed ip_next as a descriptor
	 * pointer and page-faults at 0xFFFFF804. See helper above. */
	sintran_init_iplrec(m->cpu);
	output(ctx, "IPL record initialized (iplrec.ip_next = -1 @ 0x30001000)");

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
	if (!nd500_machine_mmu_is_enabled(m)) {
		output(ctx, "NOTE: MMU is disabled - PST entries exist but are not active");
		output(ctx, "");
	}
	output(ctx, "PSN   Mode  PFN     Physical Address  Max Size");
	output(ctx, "----  ----  ------  ----------------  --------");

	int count = 0;
	for (uint32_t psn = 0; psn < MAX_PST; psn++) {
		PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(m->cpu, psn);

		/* Only show non-zero entries */
		if (pst.index_mode != 0 || pst.physical_pfn != 0) {
			const char* mode_str;
			const char* size_str;
			switch (pst.index_mode) {
				case PS_AZI: mode_str = "AZI "; size_str = "2KB"; break;
				case PS_ASI: mode_str = "ASI "; size_str = "1MB"; break;
				case PS_ADI: mode_str = "ADI "; size_str = "128MB"; break;
				default: mode_str = "??? "; size_str = "?"; break;
			}

			output(ctx, "%4u  %s  0x%04X  0x%08X        %s",
				psn, mode_str, pst.physical_pfn, pst.physical_pfn << PGSHIFT, size_str);
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
		output(ctx, "");
		output(ctx, "Index Modes: AZI=Direct(2KB), ASI=Single-level(1MB), ADI=Two-level(128MB)");
	}

	return 0;
}

static int cmd_listpcb(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	/* PCB capabilities only meaningful when MMU is enabled */
	if (!nd500_machine_mmu_is_enabled(m)) {
		error(ctx, "MMU is disabled - PCB capabilities only exist when MMU is enabled");
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
					/* PC_DIR is 0x0000, check PC_IND instead */
					if ((pc & PC_IND) == 0) 
						strcat(desc, ",DIR");
					else 
						strcat(desc, ",IND");
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

/* Dump page table entries for a PST segment */
static int cmd_dumppt(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "no cpu linked");
		return -1;
	}

	/* Page tables only meaningful when MMU is enabled */
	if (!nd500_machine_mmu_is_enabled(m)) {
		error(ctx, "MMU is disabled - page tables only meaningful when MMU is enabled");
		return -1;
	}

	char* psn_str = args ? strtok(args, " \t\r\n") : NULL;
	char* start_str = psn_str ? strtok(NULL, " \t\r\n") : NULL;
	char* count_str = start_str ? strtok(NULL, " \t\r\n") : NULL;

	if (!psn_str) {
		error(ctx, "usage: dumppt <psn> [start] [count]");
		error(ctx, "  psn:   Physical Segment Number");
		error(ctx, "  start: Starting index (default 0)");
		error(ctx, "  count: Number of entries (default: all valid, max 64)");
		return -1;
	}

	uint32_t psn = nd500_cmd_parse_u32(psn_str, 0);
	if (psn >= MAX_PST) {
		error(ctx, "PSN out of range (0-%d)", MAX_PST - 1);
		return -1;
	}

	PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(m->cpu, psn);

	if (pst.index_mode == PS_AZI) {
		output(ctx, "PSN %u uses PS_AZI (direct mapping) - no page table", psn);
		output(ctx, "Physical frame: 0x%04X -> 0x%08X", pst.physical_pfn, pst.physical_pfn << PGSHIFT);
		return 0;
	}

	/* Determine max entries based on mode */
	int max_entries = (pst.index_mode == PS_ASI) ? 512 : 128;
	const char* index_name = (pst.index_mode == PS_ASI) ? "L2" : "L1";

	int start = start_str ? (int)nd500_cmd_parse_u32(start_str, 0) : 0;
	int count = count_str ? (int)nd500_cmd_parse_u32(count_str, 64) : 64;

	if (start >= max_entries) {
		error(ctx, "Start index %d out of range (0-%d)", start, max_entries - 1);
		return -1;
	}

	/* Clamp count */
	if (count > 64) count = 64;
	if (start + count > max_entries) count = max_entries - start;

	uint32_t pt_base = pst.physical_pfn << PGSHIFT;

	if (pst.index_mode == PS_ASI) {
		output(ctx, "=== Page Table for PSN %u (PS_ASI - Single Level) ===", psn);
		output(ctx, "Page table at: 0x%08X", pt_base);
		output(ctx, "Max entries: 512 (L2 index 0-511)");
	} else {
		output(ctx, "=== L1 Page Table for PSN %u (PS_ADI - Two Level) ===", psn);
		output(ctx, "L1 table at: 0x%08X", pt_base);
		output(ctx, "Max L1 entries: 128 (L1 index 0-127)");
		output(ctx, "Each L1 entry points to an L2 table with 512 entries");
	}

	output(ctx, "");
	output(ctx, "%s Idx  PTE Addr    Raw PTE     PFN     Physical    Prot  Valid", index_name);
	output(ctx, "------  ----------  ----------  ------  ----------  ----  -----");

	int valid_count = 0;
	for (int i = start; i < start + count; i++) {
		uint32_t pte_addr = pt_base + (i * 4);
		PageTableEntry pte = nd500_mmu_read_pte(m->cpu, pte_addr);

		/* Read raw PTE value for display */
		uint32_t raw = (uint32_t)nd500_bus_read8(m, pte_addr) << 24;
		raw |= (uint32_t)nd500_bus_read8(m, pte_addr + 1) << 16;
		raw |= (uint32_t)nd500_bus_read8(m, pte_addr + 2) << 8;
		raw |= (uint32_t)nd500_bus_read8(m, pte_addr + 3);

		const char* prot_str = pte.protection ? "RO" : "RW";
		const char* valid_str = pte.valid ? "Yes" : "No";

		/* Only show if valid, or if explicitly requested range */
		if (pte.valid || count_str) {
			output(ctx, "%6d  0x%08X  0x%08X  0x%04X  0x%08X  %s    %s",
				i, pte_addr, raw, pte.physical_pfn,
				pte.physical_pfn << PGSHIFT, prot_str, valid_str);
			if (pte.valid) valid_count++;
		}
	}

	output(ctx, "");
	output(ctx, "Showing entries %d-%d, %d valid", start, start + count - 1, valid_count);

	if (pst.index_mode == PS_ADI) {
		output(ctx, "");
		output(ctx, "Note: For PS_ADI, each valid L1 entry points to an L2 page table.");
		output(ctx, "To view L2 entries, read the PFN and use: m <paddr> 2048");
	}

	return 0;
}

/* ========================================================================
 * DOM VERIFICATION COMMAND
 *
 * Verifies that DOM file data in memory matches the original disk file.
 * Compares both physical memory and virtual memory (through MMU).
 * ======================================================================== */
static int cmd_domverify(Nd500Machine* m, CmdContext* ctx, char* args) {
	(void)args;

	if (!ndlib_dom_is_loaded()) {
		error(ctx, "No DOM file loaded. Use --dom <path> to load one.");
		return -1;
	}

	int is_dom = ndlib_dom_is_dom_file();
	int max_segs = is_dom ? 32 : 1;
	const char* filepath = ndlib_get_dom_filepath();

	output(ctx, "=== DOM File Verification ===");
	output(ctx, "File: %s", filepath ? filepath : "(unknown)");
	output(ctx, "Type: %s", is_dom ? "DOM (Domain)" : "SEG (Segment)");
	output(ctx, "");

	/* Calculate physical layout same as loader does */
	uint32_t phys_data_base = 0x00000000;
	uint32_t total_data_size = 0;
	uint32_t total_prog_size = 0;

	/* First pass: measure DATA sections */
	for (int i = 0; i < max_segs; i++) {
		uint32_t dat_size, dat_addr;
		const uint8_t* dat_data = ndlib_dom_get_data_section(i, &dat_size, &dat_addr);
		if (dat_data && dat_size > 0) {
			total_data_size += dat_size;
		}
	}

	/* Calculate PROG base (page-aligned after DATA) */
	uint32_t phys_prog_base = (phys_data_base + total_data_size + 0x7FF) & ~0x7FFu;

	/* Reset for actual verification */
	total_data_size = 0;
	total_prog_size = 0;

	int total_errors = 0;
	int segments_checked = 0;

	/* Verify DATA sections */
	output(ctx, "--- DATA Sections ---");
	for (int i = 0; i < max_segs; i++) {
		uint32_t dat_size, dat_addr;
		const uint8_t* dat_data = ndlib_dom_get_data_section(i, &dat_size, &dat_addr);
		if (dat_data && dat_size > 0) {
			uint32_t phys_addr = phys_data_base + total_data_size;
			int errors = 0;
			int first_error_offset = -1;
			uint8_t first_expected = 0, first_actual = 0;

			/* Compare against physical memory */
			for (uint32_t j = 0; j < dat_size; j++) {
				uint8_t expected = dat_data[j];
				uint8_t actual = nd500_bus_read8(m, phys_addr + j);
				if (expected != actual) {
					if (first_error_offset < 0) {
						first_error_offset = (int)j;
						first_expected = expected;
						first_actual = actual;
					}
					errors++;
				}
			}

			if (errors == 0) {
				output(ctx, "Seg[%d] DATA: %u bytes @ phys 0x%08X - OK (FLA=0x%08X)",
				       i, dat_size, phys_addr, dat_addr);
			} else {
				output(ctx, "Seg[%d] DATA: %u bytes @ phys 0x%08X - FAILED: %d mismatches",
				       i, dat_size, phys_addr, errors);
				output(ctx, "         First error at offset %d: expected 0x%02X, got 0x%02X",
				       first_error_offset, first_expected, first_actual);
				total_errors += errors;
			}

			total_data_size += dat_size;
			segments_checked++;
		}
	}

	/* Verify PROG sections */
	output(ctx, "");
	output(ctx, "--- PROG Sections ---");
	for (int i = 0; i < max_segs; i++) {
		uint32_t seg_size, seg_addr;
		const uint8_t* seg_data = ndlib_dom_get_segment_data(i, &seg_size, &seg_addr);
		if (seg_data && seg_size > 0) {
			uint32_t phys_addr = phys_prog_base + total_prog_size;
			int errors = 0;
			int first_error_offset = -1;
			uint8_t first_expected = 0, first_actual = 0;

			/* Compare against physical memory */
			for (uint32_t j = 0; j < seg_size; j++) {
				uint8_t expected = seg_data[j];
				uint8_t actual = nd500_bus_read8(m, phys_addr + j);
				if (expected != actual) {
					if (first_error_offset < 0) {
						first_error_offset = (int)j;
						first_expected = expected;
						first_actual = actual;
					}
					errors++;
				}
			}

			if (errors == 0) {
				output(ctx, "Seg[%d] PROG: %u bytes @ phys 0x%08X - OK (FLA=0x%08X)",
				       i, seg_size, phys_addr, seg_addr);
			} else {
				output(ctx, "Seg[%d] PROG: %u bytes @ phys 0x%08X - FAILED: %d mismatches",
				       i, seg_size, phys_addr, errors);
				output(ctx, "         First error at offset %d: expected 0x%02X, got 0x%02X",
				       first_error_offset, first_expected, first_actual);
				total_errors += errors;
			}

			total_prog_size += seg_size;
			segments_checked++;
		}
	}

	/* Summary */
	output(ctx, "");
	output(ctx, "=== Summary ===");
	output(ctx, "Segments checked: %d", segments_checked);
	output(ctx, "Total DATA: %u bytes @ phys 0x%08X..0x%08X",
	       total_data_size, phys_data_base,
	       total_data_size > 0 ? phys_data_base + total_data_size - 1 : 0);
	output(ctx, "Total PROG: %u bytes @ phys 0x%08X..0x%08X",
	       total_prog_size, phys_prog_base,
	       total_prog_size > 0 ? phys_prog_base + total_prog_size - 1 : 0);

	if (total_errors == 0) {
		output(ctx, "Result: ALL OK - Memory matches disk file");
	} else {
		output(ctx, "Result: FAILED - %d byte mismatches found", total_errors);
	}

	return total_errors > 0 ? -1 : 0;
}

/* ========================================================================
 * MON CALL DEBUGGER COMMANDS
 *
 * Commands for controlling SINTRAN MON call emulation:
 *   mon log [off|error|warn|info|debug|trace]  - Set logging level
 *   mon status                                  - Show implementation status
 *   mon list [validated|inprogress|notimpl]    - List MON calls by status
 *   mon info <number>                           - Show MON call details
 *   mon break [unimpl|inprog|off]               - Set break behavior
 * ======================================================================== */

/* Callback to print MON entries during enumeration */
static void mon_list_callback(const MonRegistryEntry* entry, void* user_data) {
	CmdContext* ctx = (CmdContext*)user_data;
	if (!entry || !ctx) return;

	output(ctx, "  %-5s %-8s %s",
		entry->octal_str ? entry->octal_str : "?",
		entry->name ? entry->name : "?",
		entry->long_name ? entry->long_name : "");
}

static int cmd_mon(Nd500Machine* m, CmdContext* ctx, char* args) {
	char* sub = args ? strtok(args, " \t\r\n") : NULL;

	if (!sub) {
		/* No subcommand - show help */
		output(ctx, "MON call emulation commands:");
		output(ctx, "  mon log [off|error|warn|info|debug|trace] - Set/show logging level");
		output(ctx, "  mon status                                 - Show implementation status");
		output(ctx, "  mon list [validated|inprogress|notimpl]    - List MON calls by status");
		output(ctx, "  mon info <number>                          - Show MON call details");
		output(ctx, "  mon break [unimpl|inprog|off]              - Set/show break behavior");
		return 0;
	}

	/* mon log [level] */
	if (strcmp(sub, "log") == 0) {
		char* level_str = strtok(NULL, " \t\r\n");
		if (!level_str) {
			/* Show current level */
			MonLogLevel cur = mon_log_get_level();
			const char* level_names[] = {"off", "error", "warn", "info", "debug", "trace"};
			int enabled = mon_log_is_enabled();
			output(ctx, "MON logging: %s (level=%s)",
				enabled ? "enabled" : "disabled",
				level_names[cur < 6 ? cur : 0]);
			return 0;
		}

		/* Parse and set level */
		MonLogLevel new_level;
		if (strcasecmp(level_str, "off") == 0) {
			mon_log_enable(0);
			output(ctx, "MON logging disabled");
			return 0;
		} else if (strcasecmp(level_str, "error") == 0) {
			new_level = MON_LOG_ERROR;
		} else if (strcasecmp(level_str, "warn") == 0) {
			new_level = MON_LOG_WARN;
		} else if (strcasecmp(level_str, "info") == 0) {
			new_level = MON_LOG_INFO;
		} else if (strcasecmp(level_str, "debug") == 0) {
			new_level = MON_LOG_DEBUG;
		} else if (strcasecmp(level_str, "trace") == 0) {
			new_level = MON_LOG_TRACE;
		} else {
			error(ctx, "usage: mon log [off|error|warn|info|debug|trace]");
			return -1;
		}

		mon_log_enable(1);
		mon_log_set_level(new_level);
		output(ctx, "MON logging set to %s", level_str);
		return 0;
	}

	/* mon status */
	if (strcmp(sub, "status") == 0) {
		int validated = mon_count_by_status(MON_STATUS_VALIDATED);
		int in_progress = mon_count_by_status(MON_STATUS_IN_PROGRESS);
		int not_impl = mon_count_by_status(MON_STATUS_NOT_IMPLEMENTED);
		int deprecated = mon_count_by_status(MON_STATUS_DEPRECATED);
		int total = mon_get_total_count();

		output(ctx, "MON Implementation Status:");
		output(ctx, "  VALIDATED:       %3d calls", validated);
		output(ctx, "  IN_PROGRESS:     %3d calls", in_progress);
		output(ctx, "  NOT_IMPLEMENTED: %3d calls", not_impl);
		output(ctx, "  DEPRECATED:      %3d calls", deprecated);
		output(ctx, "  Total:           %3d calls", total);

		/* Show current behavior settings */
		MonUnimplBehavior unimpl_beh = mon_get_unimpl_behavior();
		MonUnimplBehavior inprog_beh = mon_get_inprogress_behavior();
		const char* beh_names[] = {"continue", "break", "halt"};

		output(ctx, "");
		output(ctx, "Behavior on unimplemented: %s", beh_names[unimpl_beh]);
		output(ctx, "Behavior on in-progress:   %s", beh_names[inprog_beh]);

		/* Show logging status */
		int log_enabled = mon_log_is_enabled();
		MonLogLevel log_level = mon_log_get_level();
		const char* level_names[] = {"off", "error", "warn", "info", "debug", "trace"};
		output(ctx, "Logging: %s (level=%s)",
			log_enabled ? "enabled" : "disabled",
			level_names[log_level < 6 ? log_level : 0]);

		return 0;
	}

	/* mon list [status] */
	if (strcmp(sub, "list") == 0) {
		char* status_str = strtok(NULL, " \t\r\n");
		MonImplStatus filter_status;
		const char* status_name;

		if (!status_str || strcasecmp(status_str, "all") == 0) {
			/* List all - show validated first, then in_progress */
			output(ctx, "VALIDATED MON calls:");
			mon_enumerate_by_status(MON_STATUS_VALIDATED, mon_list_callback, ctx);

			output(ctx, "");
			output(ctx, "IN_PROGRESS MON calls:");
			mon_enumerate_by_status(MON_STATUS_IN_PROGRESS, mon_list_callback, ctx);
			return 0;
		} else if (strcasecmp(status_str, "validated") == 0) {
			filter_status = MON_STATUS_VALIDATED;
			status_name = "VALIDATED";
		} else if (strcasecmp(status_str, "inprogress") == 0 || strcasecmp(status_str, "in_progress") == 0) {
			filter_status = MON_STATUS_IN_PROGRESS;
			status_name = "IN_PROGRESS";
		} else if (strcasecmp(status_str, "notimpl") == 0 || strcasecmp(status_str, "not_implemented") == 0) {
			filter_status = MON_STATUS_NOT_IMPLEMENTED;
			status_name = "NOT_IMPLEMENTED";
		} else if (strcasecmp(status_str, "deprecated") == 0) {
			filter_status = MON_STATUS_DEPRECATED;
			status_name = "DEPRECATED";
		} else {
			error(ctx, "usage: mon list [all|validated|inprogress|notimpl|deprecated]");
			return -1;
		}

		int count = mon_count_by_status(filter_status);
		output(ctx, "%s MON calls (%d):", status_name, count);
		mon_enumerate_by_status(filter_status, mon_list_callback, ctx);
		return 0;
	}

	/* mon info <number|name> */
	if (strcmp(sub, "info") == 0) {
		char* arg_str = strtok(NULL, " \t\r\n");
		if (!arg_str) {
			error(ctx, "usage: mon info <number|name>");
			return -1;
		}

		const MonRegistryEntry* entry = NULL;
		size_t len = strlen(arg_str);

		/* Try parsing as number first */
		if (len > 1 && (arg_str[len-1] == 'B' || arg_str[len-1] == 'b')) {
			/* Octal format like "11B" */
			char octal_buf[32];
			strncpy(octal_buf, arg_str, len - 1);
			octal_buf[len - 1] = '\0';
			char* endptr;
			uint32_t mon_num = (uint32_t)strtoul(octal_buf, &endptr, 8);
			if (*endptr == '\0') {
				entry = mon_get_entry(mon_num);
			}
		} else if (arg_str[0] >= '0' && arg_str[0] <= '9') {
			/* Starts with digit - try decimal or hex */
			char* endptr;
			uint32_t mon_num = (uint32_t)strtoul(arg_str, &endptr, 0);
			if (*endptr == '\0') {
				entry = mon_get_entry(mon_num);
			}
		}

		/* If not found by number, try by name */
		if (!entry) {
			entry = mon_get_entry_by_name(arg_str);
		}

		if (!entry) {
			error(ctx, "MON '%s' not found in registry", arg_str);
			return -1;
		}

		const char* status_str;
		switch (entry->status) {
			case MON_STATUS_VALIDATED:       status_str = "VALIDATED"; break;
			case MON_STATUS_IN_PROGRESS:     status_str = "IN_PROGRESS"; break;
			case MON_STATUS_DEPRECATED:      status_str = "DEPRECATED"; break;
			case MON_STATUS_NOT_IMPLEMENTED:
			default:                         status_str = "NOT_IMPLEMENTED"; break;
		}

		output(ctx, "MON %s (%u decimal):", entry->octal_str, entry->mon_number);
		output(ctx, "  Name:        %s", entry->name ? entry->name : "(unknown)");
		output(ctx, "  Long name:   %s", entry->long_name ? entry->long_name : "(unknown)");
		output(ctx, "  Description: %s", entry->description ? entry->description : "(none)");
		output(ctx, "  Parameters:  %u", entry->param_count);

		/* Show parameter details if available */
		if (entry->params_desc && entry->params_desc[0] != '\0') {
			output(ctx, "");
			output(ctx, "  Parameter details:");
			/* params_desc contains escaped \n sequences, split on them */
			const char* p = entry->params_desc;
			while (*p) {
				const char* line_end = p;
				/* Look for literal backslash-n sequence or actual newline */
				while (*line_end && !(*line_end == '\\' && *(line_end+1) == 'n') && *line_end != '\n') {
					line_end++;
				}
				int line_len = (int)(line_end - p);
				if (line_len > 0) {
					output(ctx, "    %.*s", line_len, p);
				}
				p = line_end;
				if (*p == '\\' && *(p+1) == 'n') p += 2;  /* Skip \n sequence */
				else if (*p == '\n') p++;  /* Skip actual newline */
			}
			output(ctx, "");
		}

		output(ctx, "  Status:      %s", status_str);
		output(ctx, "  ND-100:      %s", entry->nd100_compat ? "Yes" : "No");
		output(ctx, "  ND-500:      %s", entry->nd500_compat ? "Yes" : "No");
		return 0;
	}

	/* mon break [behavior] */
	if (strcmp(sub, "break") == 0) {
		char* beh_str = strtok(NULL, " \t\r\n");
		if (!beh_str) {
			/* Show current behavior */
			MonUnimplBehavior unimpl_beh = mon_get_unimpl_behavior();
			MonUnimplBehavior inprog_beh = mon_get_inprogress_behavior();
			const char* beh_names[] = {"continue", "break", "halt"};
			output(ctx, "Break behavior:");
			output(ctx, "  On unimplemented: %s", beh_names[unimpl_beh]);
			output(ctx, "  On in-progress:   %s", beh_names[inprog_beh]);
			return 0;
		}

		/* Set behavior */
		if (strcasecmp(beh_str, "off") == 0 || strcasecmp(beh_str, "continue") == 0) {
			mon_set_unimpl_behavior(MON_UNIMPL_CONTINUE);
			mon_set_inprogress_behavior(MON_UNIMPL_CONTINUE);
			output(ctx, "MON break behavior: continue (no breaks)");
		} else if (strcasecmp(beh_str, "unimpl") == 0) {
			mon_set_unimpl_behavior(MON_UNIMPL_BREAK);
			mon_set_inprogress_behavior(MON_UNIMPL_CONTINUE);
			output(ctx, "MON break behavior: break on unimplemented only");
		} else if (strcasecmp(beh_str, "inprog") == 0) {
			mon_set_unimpl_behavior(MON_UNIMPL_BREAK);
			mon_set_inprogress_behavior(MON_UNIMPL_BREAK);
			output(ctx, "MON break behavior: break on unimplemented and in-progress");
		} else if (strcasecmp(beh_str, "halt") == 0) {
			mon_set_unimpl_behavior(MON_UNIMPL_HALT);
			mon_set_inprogress_behavior(MON_UNIMPL_HALT);
			output(ctx, "MON break behavior: halt on unimplemented and in-progress");
		} else {
			error(ctx, "usage: mon break [off|continue|unimpl|inprog|halt]");
			return -1;
		}
		return 0;
	}

	/* Unknown subcommand */
	error(ctx, "unknown mon subcommand: %s", sub);
	error(ctx, "use 'mon' without arguments to see available commands");
	return -1;
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

/* ═══════════════════════════════════════════════════════════════════════════
 * DOMAIN COMMAND - Unified domain management
 * Usage:
 *   domain              - Show current context + list loaded domains
 *   domain <n>          - Show details for domain n
 *   domain switch <n>   - Switch execution to domain n
 *   domain symbols <n>  - Set symbol lookup domain to n
 * ═══════════════════════════════════════════════════════════════════════════ */
static int cmd_domain(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "DOMAIN: ND-500 CPU required");
		return -1;
	}
	Nd500Cpu* cpu = m->cpu;

	/* Parse arguments */
	char* arg1 = args ? strtok(args, " \t\r\n") : NULL;
	char* arg2 = arg1 ? strtok(NULL, " \t\r\n") : NULL;

	/* Check for subcommands */
	if (arg1 && strcasecmp(arg1, "switch") == 0) {
		/* domain switch <n> */
		if (!arg2) {
			error(ctx, "usage: domain switch <n>");
			return -1;
		}
		uint32_t domain = nd500_cmd_parse_u32(arg2, 0);
		if (domain > 255) {
			error(ctx, "invalid domain: %s (must be 0-255)", arg2);
			return -1;
		}

		/* Check if domain is loaded */
		if (!g_loaded_domains[domain].is_loaded) {
			error(ctx, "domain %u not loaded", domain);
			return -1;
		}

		/* Switch to domain: set CED, CAD, and PC */
		cpu->CED = (uint8_t)domain;
		cpu->CAD = (uint8_t)domain;
		cpu->PC = g_loaded_domains[domain].entry_point;
		cpu->THA = g_loaded_domains[domain].trap_handler;

		output(ctx, "Switched to domain %u (%s)", domain, g_loaded_domains[domain].domain_name);
		output(ctx, "  PC = 0x%08X, THA = 0x%08X", cpu->PC, cpu->THA);
		return 0;
	}

	if (arg1 && strcasecmp(arg1, "symbols") == 0) {
		/* domain symbols <n> - set symbol lookup domain */
		if (!arg2) {
			/* Show current symbol domain */
			output(ctx, "Symbol lookup domain: %u", cpu->symbol_domain);
			output(ctx, "Usage: domain symbols <n>");
			return 0;
		}
		uint32_t domain = nd500_cmd_parse_u32(arg2, 0);
		if (domain > 255) {
			error(ctx, "invalid domain: %s (must be 0-255)", arg2);
			return -1;
		}
		cpu->symbol_domain = (uint8_t)domain;
		output(ctx, "Symbol lookup now uses domain %u", domain);
		return 0;
	}

	/* Check if argument is a number: domain <n> - show details */
	if (arg1) {
		char* endptr;
		unsigned long domain = strtoul(arg1, &endptr, 0);
		if (*endptr == '\0' && domain <= 255) {
			if (!g_loaded_domains[domain].is_loaded) {
				error(ctx, "domain %lu not loaded", domain);
				return -1;
			}

			LoadedDomainInfo* info = &g_loaded_domains[domain];
			const char* status = (domain == cpu->CED) ? "executing" : "loaded";
			output(ctx, "Domain %lu: %s", domain, info->domain_name);
			output(ctx, "  Entry:      0x%08X", info->entry_point);
			output(ctx, "  THA:        0x%08X", info->trap_handler);
			output(ctx, "  Segments:   %d", info->segment_count);
			output(ctx, "  Status:     %s", status);
			return 0;
		}

		error(ctx, "unknown: %s. Use: domain [n|switch n|symbols n]", arg1);
		return -1;
	}

	/* No arguments: show current context + list all loaded domains */
	output(ctx, "============================================================");
	output(ctx, "  Domain Status");
	output(ctx, "============================================================");
	output(ctx, "");
	output(ctx, "  Current Context");
	output(ctx, "  ---------------");
	output(ctx, "  CED (executing): %u    CAD (alternative): %u    Symbols: %u",
	       cpu->CED, cpu->CAD, cpu->symbol_domain);
	output(ctx, "");

	/* List loaded domains */
	output(ctx, "  Loaded Domains");
	output(ctx, "  --------------");

	int any_loaded = 0;
	for (int i = 0; i < MAX_DOMAINS; i++) {
		if (!g_loaded_domains[i].is_loaded) continue;
		any_loaded = 1;

		const char* status = "";
		if (i == cpu->CED) status = " [executing]";
		else if (i == cpu->CAD && cpu->CAD != cpu->CED) status = " [alt]";

		output(ctx, "  %3d  %-16s  Entry: 0x%08X  Segs: %d%s",
		       i, g_loaded_domains[i].domain_name,
		       g_loaded_domains[i].entry_point,
		       g_loaded_domains[i].segment_count, status);
	}

	if (!any_loaded) {
		output(ctx, "  (none)");
	}

	output(ctx, "");
	output(ctx, "  Commands: domain <n>, domain switch <n>, domain symbols <n>");
	output(ctx, "============================================================");
	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * HEAP COMMAND - Dump heap variables at TOS
 * Heap Variables layout (ND-500 buddy system):
 *   TOS+0   MAXL      Max log2 size of allocatable blocks
 *   TOS+4   STAH      Start address of heap pool
 *   TOS+8   ENDH      End address of heap pool
 *   TOS+12  FLOG[0]   Freelist head for 2^0 = 1 word blocks
 *   TOS+16  FLOG[1]   Freelist head for 2^1 = 2 word blocks
 *   ...
 *   TOS+12+n*4  FLOG[n]  Freelist head for 2^n word blocks
 * ═══════════════════════════════════════════════════════════════════════════ */
static uint32_t read_virtual_word(Nd500Machine* m, uint32_t vaddr) {
	/* Read a 32-bit word via data MMU */
	uint32_t paddr;
	if (nd500_mmu_is_data_enabled(m->cpu)) {
		paddr = nd500_mmu_translate(m->cpu, vaddr, 0, 0);  /* data read */
	} else {
		paddr = vaddr;
	}
	return nd500_bus_read32(m, paddr);
}

static int cmd_heap(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "HEAP: ND-500 CPU required");
		return -1;
	}
	Nd500Cpu* cpu = m->cpu;

	uint32_t tos = cpu->TOS;
	if (tos == 0) {
		output(ctx, "No heap (TOS=0x00000000)");
		return 0;
	}

	/* Read heap header fields using data MMU */
	uint32_t maxl = read_virtual_word(m, tos + 0);
	uint32_t stah = read_virtual_word(m, tos + 4);
	uint32_t endh = read_virtual_word(m, tos + 8);

	/* Limit maxl to reasonable value */
	uint32_t maxl_display = (maxl > 31) ? 31 : maxl;
	uint32_t max_block_words = (maxl <= 31) ? (1u << maxl) : 0;

	output(ctx, "Heap Variables at TOS=0x%08X:", tos);
	output(ctx, "  +0  MAXL  = 0x%08X    (max log2 size: %u, max block = 2^%u = %u words)",
	       maxl, maxl, maxl, max_block_words);
	output(ctx, "  +4  STAH  = 0x%08X    (heap start)", stah);
	output(ctx, "  +8  ENDH  = 0x%08X    (heap end)", endh);
	if (endh > stah) {
		output(ctx, "       Heap size: %u bytes (%u words)", endh - stah, (endh - stah) / 4);
	}
	output(ctx, "");
	output(ctx, "Free Lists:");

	int any_free = 0;
	for (uint32_t i = 0; i <= maxl_display; i++) {
		uint32_t flog_offset = 12 + (i * 4);
		uint32_t flog_val = read_virtual_word(m, tos + flog_offset);

		/* Calculate block size for this freelist */
		uint32_t block_words = (1u << i);
		uint32_t block_bytes = block_words * 4;

		if (flog_val != 0) {
			/* Count blocks in this freelist (limit to avoid infinite loops) */
			int count = 0;
			uint32_t ptr = flog_val;
			while (ptr != 0 && count < 100) {
				count++;
				ptr = read_virtual_word(m, ptr);
			}
			output(ctx, "  +%u FLOG[%2u] = 0x%08X  (2^%u = %5u words = %6u bytes, %d block%s)",
			       flog_offset, i, flog_val, i, block_words, block_bytes,
			       count, (count == 1) ? "" : "s");
			any_free = 1;
		}
	}

	if (!any_free) {
		output(ctx, "  (all freelists empty)");
	}

	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * STACKFRAME / SF COMMAND - Dump current stack frame structure at B register
 * Stack frame layout (ND-500):
 *   B+0   PREVB   Previous B register value
 *   B+4   RETA    Return address
 *   B+8   SP      Stack pointer (next free location)
 *   B+12  AUX     Auxiliary field
 *   B+16  N       Number of arguments
 *   B+20+ ARGn    Argument addresses
 * ═══════════════════════════════════════════════════════════════════════════ */
static int cmd_stackframe(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "STACKFRAME: ND-500 CPU required");
		return -1;
	}
	Nd500Cpu* cpu = m->cpu;

	uint32_t b = cpu->B;
	if (b == 0) {
		output(ctx, "No stack frame (B=0x00000000)");
		return 0;
	}

	/* Read stack frame fields using data MMU */
	uint32_t prevb = read_virtual_word(m, b + 0);
	uint32_t reta = read_virtual_word(m, b + 4);
	uint32_t sp = read_virtual_word(m, b + 8);
	uint32_t aux = read_virtual_word(m, b + 12);
	uint32_t n = read_virtual_word(m, b + 16);

	output(ctx, "Stack Frame at B=0x%08X:", b);
	output(ctx, "  +0  PREVB = 0x%08X    (previous frame)", prevb);
	output(ctx, "  +4  RETA  = 0x%08X    (return address)", reta);
	output(ctx, "  +8  SP    = 0x%08X    (next free)", sp);
	output(ctx, "  +12 AUX   = 0x%08X    (auxiliary)", aux);
	output(ctx, "  +16 N     = 0x%08X    (arg count: %u)", n, n);

	/* Display arguments (limit to reasonable number) */
	uint32_t max_args = (n > 32) ? 32 : n;
	for (uint32_t i = 0; i < max_args; i++) {
		uint32_t arg_offset = 20 + (i * 4);
		uint32_t arg_val = read_virtual_word(m, b + arg_offset);
		output(ctx, "  +%u ARG%u  = 0x%08X", arg_offset, i + 1, arg_val);
	}
	if (n > 32) {
		output(ctx, "  ... (%u more arguments)", n - 32);
	}

	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * UNLOAD COMMAND - Unload domain and free resources
 * Usage: unload <domain>
 *
 * Frees:
 *   - Physical memory pages
 *   - PST entries
 *   - PCB capabilities
 *   - Domain allocation
 *
 * Note: Cannot unload domain 0 (kernel)
 * ═══════════════════════════════════════════════════════════════════════════ */
static int cmd_unload(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "UNLOAD: ND-500 CPU required");
		return -1;
	}
	Nd500Cpu* cpu = m->cpu;

	/* Parse arguments */
	char* arg = args ? strtok(args, " \t\r\n") : NULL;

	if (!arg) {
		output(ctx, "Usage: unload <domain>");
		output(ctx, "  domain - Domain number (1-255) to unload");
		output(ctx, "");
		output(ctx, "Note: Cannot unload domain 0 (kernel)");
		return 0;
	}

	uint32_t domain = nd500_cmd_parse_u32(arg, 0);
	if (domain > 255) {
		error(ctx, "Invalid domain number: %s (must be 0-255)", arg);
		return -1;
	}

	if (domain == 0) {
		error(ctx, "Cannot unload domain 0 (kernel)");
		return -1;
	}

	/* Check if domain is loaded */
	if (!g_loaded_domains[domain].is_loaded) {
		error(ctx, "Domain %u is not loaded", domain);
		return -1;
	}

	/* Check if it's the current executing domain */
	if (domain == cpu->CED) {
		error(ctx, "Cannot unload currently executing domain (CED=%u)", cpu->CED);
		return -1;
	}

	/* Save info for output before clearing */
	char domain_name[MAX_DOMAIN_NAME];
	strncpy(domain_name, g_loaded_domains[domain].domain_name, MAX_DOMAIN_NAME - 1);
	domain_name[MAX_DOMAIN_NAME - 1] = '\0';
	uint32_t entry_point = g_loaded_domains[domain].entry_point;

	/* Free the domain allocation in CPU */
	nd500_domain_free(cpu, (uint8_t)domain);

	/* Clear the debugger tracking entry */
	memset(&g_loaded_domains[domain], 0, sizeof(LoadedDomainInfo));

	/* Output result */
	output(ctx, "============================================================");
	output(ctx, "  Domain %u Unloaded", domain);
	output(ctx, "============================================================");
	output(ctx, "");
	output(ctx, "  Name:         %s", domain_name[0] ? domain_name : "(unnamed)");
	output(ctx, "  Entry Point:  0x%08X", entry_point);
	output(ctx, "");
	output(ctx, "  Resources freed:");
	output(ctx, "    - Domain allocation");
	output(ctx, "    - Memory pages (physical memory reclaimed)");
	output(ctx, "");
	output(ctx, "  Domain slot %u now available for reuse", domain);
	output(ctx, "============================================================");

	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper: Get segment name for well-known segments
 * ═══════════════════════════════════════════════════════════════════════════ */
static const char* get_segment_name(int segment, uint8_t domain) {
	if (domain == 0) {
		/* Kernel domain */
		switch (segment) {
			case 0:  return "KDATA";
			case 1:  return "KTEXT";
			case 27: return "PST";
			case 28: return "PCB";
			case 29: return "KSTACK";
			default: return NULL;
		}
	} else {
		/* User domains */
		switch (segment) {
			case 0:  return "DATA";   /* FORTRAN compatibility alias */
			case 1:  return "TEXT";
			case 26: return "UTEXT";
			case 30: return "UDATA";
			case 31: return "USTACK";
			default: return NULL;
		}
	}
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SHOWCAP COMMAND - Show capability tables for a domain
 * Usage: showcap [domain]
 * If no domain specified, shows current domain (CAD)
 * ═══════════════════════════════════════════════════════════════════════════ */
static int cmd_showcap(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "SHOWCAP: ND-500 CPU required");
		return -1;
	}
	Nd500Cpu* cpu = m->cpu;

	if (!nd500_machine_mmu_is_enabled(m)) {
		error(ctx, "MMU is disabled - capabilities only exist when MMU is enabled");
		return -1;
	}

	/* Parse arguments */
	char* arg = args ? strtok(args, " \t\r\n") : NULL;
	uint8_t target_domain = cpu->CAD;  /* Default to current domain */

	if (arg) {
		uint32_t d = nd500_cmd_parse_u32(arg, 0);
		if (d > 255) {
			error(ctx, "Invalid domain number '%s'. Must be 0-255", arg);
			return -1;
		}
		target_domain = (uint8_t)d;
	}

	output(ctx, "");
	output(ctx, "-------------------------------------------------------");
	output(ctx, "  Domain %u Capability Tables", target_domain);
	output(ctx, "-------------------------------------------------------");
	output(ctx, "");

	/* Program Capabilities */
	output(ctx, "Program Capabilities (Instruction Fetch):");
	output(ctx, "Seg  Raw   Type      Target              Description");
	output(ctx, "---  ----  --------  ------------------  -----------");

	int prog_cap_count = 0;
	for (int seg = 0; seg < 32; seg++) {
		uint16_t cap = nd500_mmu_get_program_capability(cpu, target_domain, seg);
		if (cap != 0) {
			int is_indirect = (cap & PC_IND) != 0;
			const char* type = is_indirect ? "INDIRECT" : "DIRECT  ";
			char target[32];
			const char* seg_name = get_segment_name(seg, target_domain);

			if (is_indirect) {
				int remote_domain = (cap >> 5) & 0xFF;
				int remote_seg = cap & 0x1F;
				snprintf(target, sizeof(target), "Domain %3d, Seg %2d", remote_domain, remote_seg);
			} else {
				int psn = cap & PC_PSN;
				snprintf(target, sizeof(target), "PSN %4d (0x%03X)", psn, psn);
			}

			output(ctx, "%3d  %04X  %s  %-18s  %s",
			       seg, cap, type, target, seg_name ? seg_name : "");
			prog_cap_count++;
		}
	}

	if (prog_cap_count == 0) {
		output(ctx, "(no program capabilities configured)");
	}

	output(ctx, "");
	output(ctx, "Data Capabilities (Data Access):");
	output(ctx, "Seg  Raw   PSN   Flags  Physical Addr   Description");
	output(ctx, "---  ----  ----  -----  --------------  -----------");

	int data_cap_count = 0;
	for (int seg = 0; seg < 32; seg++) {
		uint16_t cap = nd500_mmu_get_data_capability(cpu, target_domain, seg);
		if (cap != 0) {
			int writable = (cap & DC_WRP) != 0;
			int user_access = (cap & DC_PAC) != 0;
			int shared = (cap & DC_SHS) != 0;
			int psn = cap & DC_PSN;

			char flags[5];
			flags[0] = writable ? 'W' : 'R';
			flags[1] = user_access ? 'U' : 'K';
			flags[2] = shared ? 'S' : '-';
			flags[3] = '\0';

			PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(cpu, psn);
			uint32_t physical_base = pst.physical_pfn << 11;  /* 2KB pages */

			const char* seg_name = get_segment_name(seg, target_domain);

			output(ctx, "%3d  %04X  %4d  %-5s  0x%08X    %s",
			       seg, cap, psn, flags, physical_base, seg_name ? seg_name : "");
			data_cap_count++;
		}
	}

	if (data_cap_count == 0) {
		output(ctx, "(no data capabilities configured)");
	}

	output(ctx, "");
	output(ctx, "Total: %d program capabilities, %d data capabilities", prog_cap_count, data_cap_count);
	output(ctx, "");
	output(ctx, "Flags: W=Writable R=ReadOnly U=UserAccess K=KernelOnly S=Shared");
	output(ctx, "");

	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SHOWPAGES COMMAND - Show page mappings for a domain
 * Usage: showpages <domain> [segment]
 * ═══════════════════════════════════════════════════════════════════════════ */
static int cmd_showpages(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "SHOWPAGES: ND-500 CPU required");
		return -1;
	}
	Nd500Cpu* cpu = m->cpu;

	if (!nd500_machine_mmu_is_enabled(m)) {
		error(ctx, "MMU is disabled - page mappings only exist when MMU is enabled");
		return -1;
	}

	/* Parse arguments */
	char* arg1 = args ? strtok(args, " \t\r\n") : NULL;
	char* arg2 = arg1 ? strtok(NULL, " \t\r\n") : NULL;

	if (!arg1) {
		output(ctx, "");
		output(ctx, "Usage: showpages <domain> [segment]");
		output(ctx, "");
		output(ctx, "Parameters:");
		output(ctx, "  domain  - Domain number (0-255)");
		output(ctx, "  segment - Optional segment number (0-31)");
		output(ctx, "");
		output(ctx, "Examples:");
		output(ctx, "  showpages 0       - Show all segments for kernel domain");
		output(ctx, "  showpages 1 1     - Show TEXT segment (1) for domain 1");
		output(ctx, "  showpages 1 0     - Show DATA segment (0) for domain 1");
		output(ctx, "");
		return 0;
	}

	uint32_t domain = nd500_cmd_parse_u32(arg1, 0);
	if (domain > 255) {
		error(ctx, "Invalid domain number '%s'. Must be 0-255", arg1);
		return -1;
	}

	int target_segment = -1;  /* -1 means show all */
	if (arg2) {
		target_segment = (int)nd500_cmd_parse_u32(arg2, 0);
		if (target_segment < 0 || target_segment > 31) {
			error(ctx, "Invalid segment number '%s'. Must be 0-31", arg2);
			return -1;
		}
	}

	output(ctx, "");
	output(ctx, "-------------------------------------------------------");
	output(ctx, "  ND-500 PAGE MAPPINGS - Domain %u", domain);
	output(ctx, "-------------------------------------------------------");
	output(ctx, "");

	int segments_shown = 0;

	for (int seg = 0; seg < 32; seg++) {
		/* Skip if specific segment requested and this isn't it */
		if (target_segment >= 0 && seg != target_segment)
			continue;

		uint16_t data_cap = nd500_mmu_get_data_capability(cpu, domain, seg);
		if (data_cap == 0)
			continue;  /* Segment not mapped */

		int writable = (data_cap & DC_WRP) != 0;
		int user_access = (data_cap & DC_PAC) != 0;
		int psn = data_cap & DC_PSN;

		const char* seg_name = get_segment_name(seg, domain);
		output(ctx, "Segment %2d (%s):", seg, seg_name ? seg_name : "");
		output(ctx, "  PSN: %d (0x%03X)", psn, psn);
		output(ctx, "  Access: %s, %s mode",
		       writable ? "Read/Write" : "Read-Only",
		       user_access ? "User" : "Kernel");

		/* Get PST entry to determine indexing mode */
		PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(cpu, psn);
		const char* index_mode;
		switch (pst.index_mode) {
			case 0: index_mode = "Direct (AZI) - No paging"; break;
			case 1: index_mode = "Single-level (ASI)"; break;
			case 2: index_mode = "Two-level (ADI)"; break;
			default: index_mode = "Unknown"; break;
		}

		output(ctx, "  Index Mode: %s", index_mode);
		output(ctx, "  Base PFN: %u (0x%X)", pst.physical_pfn, pst.physical_pfn);

		/* Calculate virtual and physical address ranges */
		uint32_t virtual_base = (uint32_t)seg << 27;  /* Segment base address */
		uint32_t physical_base = pst.physical_pfn << 11;  /* Physical base (2KB pages) */

		output(ctx, "  Virtual Base:  0x%08X", virtual_base);
		output(ctx, "  Physical Base: 0x%08X", physical_base);

		/* For direct mapped segments, show simple mapping */
		if (pst.index_mode == 0) {
			output(ctx, "  Direct mapping: Virtual 0x%08X -> Physical 0x%08X", virtual_base, physical_base);
		}

		output(ctx, "");
		segments_shown++;
	}

	if (segments_shown == 0) {
		if (target_segment >= 0)
			output(ctx, "Segment %d is not mapped in domain %u", target_segment, domain);
		else
			output(ctx, "No segments mapped in domain %u", domain);
	} else {
		output(ctx, "Total: %d segment(s) mapped", segments_shown);
	}

	output(ctx, "");
	return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * MEMMAP COMMAND - Display memory map (virtual or physical)
 * Usage: memmap [domain|phys]
 * ═══════════════════════════════════════════════════════════════════════════ */
static int cmd_memmap(Nd500Machine* m, CmdContext* ctx, char* args) {
	if (!m || !m->cpu) {
		error(ctx, "MEMMAP: ND-500 CPU required");
		return -1;
	}
	Nd500Cpu* cpu = m->cpu;

	if (!nd500_machine_mmu_is_enabled(m)) {
		error(ctx, "MMU is disabled - memory map only available when MMU is enabled");
		return -1;
	}

	/* Parse arguments */
	char* arg = args ? strtok(args, " \t\r\n") : NULL;

	if (arg && strcasecmp(arg, "phys") == 0) {
		/* Physical memory overview */
		output(ctx, "");
		output(ctx, "ND-500 Physical Memory Overview:");
		output(ctx, "");

		/* Calculate memory usage from loaded domains */
		uint32_t total_pages = 0;
		int domain_count = 0;

		output(ctx, "Allocation by Domain:");
		output(ctx, "+--------+---------------------+--------+----------+");
		output(ctx, "| Domain | Name                | Pages  | Size     |");
		output(ctx, "+--------+---------------------+--------+----------+");

		for (int d = 0; d < MAX_DOMAINS; d++) {
			if (!g_loaded_domains[d].is_loaded) continue;

			/* Estimate pages from segment count (rough approximation) */
			/* In reality we'd need to track actual page allocations */
			uint32_t domain_pages = 0;

			/* Count pages from data capabilities */
			for (int seg = 0; seg < 32; seg++) {
				uint16_t dc = nd500_mmu_get_data_capability(cpu, d, seg);
				if (dc != 0) {
					int psn = dc & DC_PSN;
					PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(cpu, psn);
					/* For ASI mode, count pages from page table */
					if (pst.index_mode == 1) {
						/* Estimate based on loaded info */
						domain_pages += 50;  /* Approximate */
					} else if (pst.index_mode == 0) {
						domain_pages += 1;  /* AZI = 1 page */
					}
				}
			}

			total_pages += domain_pages;
			domain_count++;

			char name[20];
			strncpy(name, g_loaded_domains[d].domain_name, 19);
			name[19] = '\0';
			if (name[0] == '\0') strcpy(name, "(unnamed)");

			output(ctx, "| %6d | %-19s | %6u | %4u KB  |",
			       d, name, domain_pages, domain_pages * 2);
		}

		output(ctx, "+--------+---------------------+--------+----------+");
		output(ctx, "");
		output(ctx, "Total: %d domain(s), ~%u pages (~%u KB) allocated",
		       domain_count, total_pages, total_pages * 2);
		output(ctx, "");
		return 0;
	}

	/* Virtual memory map for a domain */
	uint8_t target_domain = cpu->CAD;  /* Default to current domain */

	if (arg) {
		uint32_t d = nd500_cmd_parse_u32(arg, 0);
		if (d > 255) {
			error(ctx, "Invalid domain number '%s'. Must be 0-255", arg);
			return -1;
		}
		target_domain = (uint8_t)d;
	}

	/* Check if domain is loaded */
	if (!g_loaded_domains[target_domain].is_loaded) {
		error(ctx, "Domain %u is not loaded. Use 'domain' to see loaded domains.", target_domain);
		return -1;
	}

	output(ctx, "Domain %u Memory Map: %s", target_domain, g_loaded_domains[target_domain].domain_name);
	output(ctx, "+-------------------+---------+--------------+--------------+----------+");
	output(ctx, "| Virtual Range     | Segment | Index Mode   | Physical PFN | Size     |");
	output(ctx, "+-------------------+---------+--------------+--------------+----------+");

	uint32_t total_pages = 0;
	int segment_count = 0;

	for (int seg = 0; seg < 32; seg++) {
		uint16_t data_cap = nd500_mmu_get_data_capability(cpu, target_domain, seg);
		if (data_cap == 0)
			continue;

		int psn = data_cap & DC_PSN;
		PhysicalSegmentTableEntry pst = nd500_mmu_get_pst_entry(cpu, psn);

		/* Calculate virtual address range */
		uint32_t virtual_base = (uint32_t)seg << 27;
		uint32_t virtual_end = virtual_base;

		/* Determine size and index mode */
		const char* index_mode = "";
		uint32_t page_count = 0;
		char pfn_str[20];

		if (pst.index_mode == 0) {
			index_mode = "AZI (Direct)";
			page_count = 1;
			snprintf(pfn_str, sizeof(pfn_str), "%u", pst.physical_pfn);
			virtual_end = virtual_base + (page_count * 2048) - 1;
		} else if (pst.index_mode == 1) {
			index_mode = "ASI (Paged) ";
			/* Estimate page count - would need segment info for exact count */
			page_count = 50;  /* Approximate */
			snprintf(pfn_str, sizeof(pfn_str), "%u+", pst.physical_pfn);
			virtual_end = virtual_base + (page_count * 2048) - 1;
		} else if (pst.index_mode == 2) {
			index_mode = "ADI (2-level)";
			page_count = 100;  /* Approximate */
			snprintf(pfn_str, sizeof(pfn_str), "%u+", pst.physical_pfn);
			virtual_end = virtual_base + (page_count * 2048) - 1;
		}

		if (page_count == 0)
			continue;

		total_pages += page_count;
		segment_count++;

		char virt_range[24];
		snprintf(virt_range, sizeof(virt_range), "%08X-%08X", virtual_base, virtual_end);

		char size_str[12];
		if (page_count * 2 >= 1024) {
			snprintf(size_str, sizeof(size_str), "%uMB", (page_count * 2) / 1024);
		} else {
			snprintf(size_str, sizeof(size_str), "%uKB", page_count * 2);
		}

		output(ctx, "| %-17s | %7d | %-12s | %-12s | %-8s |",
		       virt_range, seg, index_mode, pfn_str, size_str);
	}

	output(ctx, "+-------------------+---------+--------------+--------------+----------+");
	output(ctx, "Total: ~%u KB (~%u pages) used by domain", total_pages * 2, total_pages);

	return 0;
}

/* ============================================================================
 * SINTRAN Files Commands
 * ============================================================================ */

/* Get access mode name */
static const char* get_access_mode_name(uint8_t mode) {
	static const char* names[] = {
		"SeqRead", "SeqWrite", "RandRead", "RandWrite", "RandRdWr",
		"SeqAppend", "SeqCommon", "RandCommon", "SeqExtend", "RandExtend"
	};
	if (mode < 10) return names[mode];
	return "Unknown";
}

/*
 * Format ND date (packed 32-bit SINTRAN format) to human-readable string
 *
 * ND Date Format:
 *   Bits 31-26 (6 bits): Year offset from 1950 (0-63, valid years: 1950-2013)
 *   Bits 25-22 (4 bits): Month (1-12)
 *   Bits 21-17 (5 bits): Day of month (1-31)
 *   Bits 16-12 (5 bits): Hour (0-23)
 *   Bits 11-6  (6 bits): Minute (0-59)
 *   Bits 5-0   (6 bits): Second (0-59)
 */
static const char* format_nd_date(uint32_t nd_date, char* buf, size_t buf_size) {
	if (nd_date == 0) {
		snprintf(buf, buf_size, "(not set)");
		return buf;
	}

	/* Extract components from packed ND date format */
	int year  = ((nd_date >> 26) & 0x3F) + 1950;
	int month = (nd_date >> 22) & 0x0F;
	int day   = (nd_date >> 17) & 0x1F;
	int hour  = (nd_date >> 12) & 0x1F;
	int min   = (nd_date >> 6) & 0x3F;
	int sec   = nd_date & 0x3F;

	/* Validate ranges */
	if (month < 1 || month > 12 || day < 1 || day > 31 ||
	    hour > 23 || min > 59 || sec > 59) {
		snprintf(buf, buf_size, "0x%08X (invalid)", nd_date);
		return buf;
	}

	snprintf(buf, buf_size, "%04d-%02d-%02d %02d:%02d:%02d",
	         year, month, day, hour, min, sec);
	return buf;
}

/* files - List all open SINTRAN files */
static int cmd_files(Nd500Machine* m, CmdContext* ctx, char* args) {
	(void)m;
	(void)args;

	int open_count = 0;
	int scratch_count = 0;

	/* First pass: count files */
	for (int fn = FILE_NUMBER_MIN; fn <= FILE_NUMBER_MAX; fn++) {
		OpenFileEntry* entry = mon_file_table_get(fn);
		if (entry && entry->in_use) {
			open_count++;
			if (entry->is_scratch) scratch_count++;
		}
	}

	if (open_count == 0) {
		output(ctx, "No open files.");
		return 0;
	}

	output(ctx, "");
	output(ctx, "Open Files (SINTRAN III):");
	output(ctx, "  FileNo  Mode        Scratch  Position      Size          Path");
	output(ctx, "  ------  ----------  -------  ------------  ------------  ----");

	for (int fn = FILE_NUMBER_MIN; fn <= FILE_NUMBER_MAX; fn++) {
		OpenFileEntry* entry = mon_file_table_get(fn);
		if (!entry || !entry->in_use) continue;

		/* Get file size from ObjectEntry */
		uint32_t size = entry->object_entry.bytes_in_file;

		output(ctx, "  %-6d  %-10s  %-7s  %-12u  %-12u  %s",
			fn,
			get_access_mode_name(entry->access_mode),
			entry->is_scratch ? "Yes" : "No",
			entry->current_position,
			size,
			entry->host_path[0] ? entry->host_path : "(none)");
	}

	output(ctx, "");
	output(ctx, "%d file(s) open (%d scratch)", open_count, scratch_count);

	return 0;
}

/* file <n> - Show details for a specific open file */
static int cmd_file(Nd500Machine* m, CmdContext* ctx, char* args) {
	(void)m;

	if (!args || !*args) {
		output(ctx, "Usage: file <file_number>");
		output(ctx, "  file_number: 64-127 (octal 100-177)");
		return 0;
	}

	/* Parse file number */
	char* endp;
	long fn = strtol(args, &endp, 0);
	if (*endp != '\0' && !isspace(*endp)) {
		error(ctx, "Invalid file number: %s", args);
		return -1;
	}

	if (!mon_file_table_is_valid_file_number((int)fn)) {
		error(ctx, "File number must be 64-127 (got %ld)", fn);
		return -1;
	}

	OpenFileEntry* entry = mon_file_table_get((int)fn);
	if (!entry || !entry->in_use) {
		error(ctx, "File %ld is not open", fn);
		return -1;
	}

	ObjectEntry* obj = &entry->object_entry;

	output(ctx, "");
	output(ctx, "File %ld Details:", fn);
	output(ctx, "  Host Path:     %s", entry->host_path[0] ? entry->host_path : "(none)");
	output(ctx, "  Access Mode:   %s (%d)", get_access_mode_name(entry->access_mode), entry->access_mode);
	output(ctx, "  Position:      %u / %u bytes", entry->current_position, obj->bytes_in_file);
	output(ctx, "  Block Size:    %u bytes", entry->block_size);
	output(ctx, "  Scratch:       %s", entry->is_scratch ? "Yes (delete on close)" : "No");

	if (entry->mapped_as_segment) {
		output(ctx, "  Mapped:        Yes (segment %u, access=%d)",
			entry->mapped_segment_no, entry->segment_access_type);
	}

	output(ctx, "");
	output(ctx, "  ObjectEntry:");

	/* Format object name (remove 0x27 terminator for display) */
	char name_buf[17] = {0};
	for (int i = 0; i < 16 && obj->object_name[i] && obj->object_name[i] != 0x27; i++) {
		name_buf[i] = obj->object_name[i];
	}

	char type_buf[5] = {0};
	for (int i = 0; i < 4 && obj->type[i] && obj->type[i] != 0x27; i++) {
		type_buf[i] = obj->type[i];
	}

	output(ctx, "    Name:        %s", name_buf[0] ? name_buf : "(empty)");
	output(ctx, "    Type:        %s", type_buf[0] ? type_buf : "(empty)");
	output(ctx, "    Header:      0x%04X", obj->header);

	/* Decode header bits */
	char header_desc[64] = "";
	if (obj->header & HEADER_USED) strcat(header_desc, "Used ");
	if (obj->header & HEADER_WRITE_OPEN) strcat(header_desc, "WriteOpen ");
	if (obj->header & HEADER_RESERVED) strcat(header_desc, "Reserved ");
	if (obj->header & HEADER_MODIFIED) strcat(header_desc, "Modified ");
	if (header_desc[0]) {
		output(ctx, "                 (%s)", header_desc);
	}

	output(ctx, "    Size:        %u bytes (%u pages)", obj->bytes_in_file, obj->pages_in_file);
	output(ctx, "    Open Count:  %u (total: %u)", obj->current_open_count, obj->total_open_count);
	output(ctx, "    Access Bits: 0x%04X", obj->access_bits);
	output(ctx, "    File Type:   0x%04X", obj->file_type);
	output(ctx, "    Device:      %u", obj->device_number);
	output(ctx, "    Object Idx:  %u", obj->object_index);

	/* Always show dates with both formatted string and raw hex for validation */
	output(ctx, "");
	output(ctx, "  Dates (SINTRAN format, valid range: 1950-2013):");
	char date_buf[32];
	output(ctx, "    Created:     %s [0x%08X]",
		format_nd_date(obj->date_created, date_buf, sizeof(date_buf)), obj->date_created);
	output(ctx, "    Last Read:   %s [0x%08X]",
		format_nd_date(obj->date_read, date_buf, sizeof(date_buf)), obj->date_read);
	output(ctx, "    Last Write:  %s [0x%08X]",
		format_nd_date(obj->date_written, date_buf, sizeof(date_buf)), obj->date_written);

	return 0;
}

/* user - Show/set current SINTRAN user */
static int cmd_user(Nd500Machine* m, CmdContext* ctx, char* args) {
	(void)m;

	/* Skip leading whitespace */
	while (args && *args && isspace((unsigned char)*args)) args++;

	if (!args || !*args) {
		/* Show current user */
		const char* current = mon_config_get_current_user();
		output(ctx, "Current SINTRAN user: %s", current ? current : "(none)");
		output(ctx, "");
		output(ctx, "Usage: user <username>");
		output(ctx, "  Sets the current user for SINTRAN path translation.");
		output(ctx, "  Example: user SYSTEM");
		output(ctx, "");
		output(ctx, "  Paths without (USER) prefix use this user:");
		output(ctx, "    FILE:DATA -> {sintran_root}/%s/FILE.DATA", current ? current : "GUEST");
		return 0;
	}

	/* Set new user */
	mon_config_set_current_user(args);
	output(ctx, "SINTRAN user set to: %s", mon_config_get_current_user());
	return 0;
}

/* ============================================================================
 * input - Queue console input for MON calls (1B, 503B, etc.)
 * ============================================================================ */
static int cmd_input(Nd500Machine* m, CmdContext* ctx, char* args) {
	(void)m;

	/* Skip leading whitespace */
	while (args && *args && isspace((unsigned char)*args)) args++;

	if (!args || !*args) {
		/* Show status and usage */
		size_t remaining = mon_get_console_input_remaining();
		size_t output_len = mon_get_console_output_len();

		output(ctx, "Console Input Queue:");
		output(ctx, "  Pending input: %zu chars", remaining);
		output(ctx, "  Output buffer: %zu chars", output_len);
		if (output_len > 0) {
			output(ctx, "  Output: \"%s\"", mon_get_console_output());
		}
		output(ctx, "");
		output(ctx, "Usage: input <text>");
		output(ctx, "  Queues text as console input for MON calls.");
		output(ctx, "  Escape sequences: \\r = CR, \\n = LF, \\\\ = backslash");
		output(ctx, "");
		output(ctx, "Examples:");
		output(ctx, "  input HELP\\r\\n     Queue 'HELP' followed by CR LF");
		output(ctx, "  input clear        Clear input queue and output buffer");
		output(ctx, "  input output       Show captured output");
		return 0;
	}

	/* Special subcommands */
	if (strcmp(args, "clear") == 0) {
		mon_clear_console_queue();
		output(ctx, "Console queue cleared.");
		return 0;
	}

	if (strcmp(args, "output") == 0) {
		size_t len = mon_get_console_output_len();
		const char* out = mon_get_console_output();
		output(ctx, "Console output (%zu chars):", len);
		if (len > 0) {
			/* Print output, escaping non-printable chars */
			char buf[256];
			size_t pos = 0;
			for (size_t i = 0; i < len && pos < sizeof(buf) - 5; i++) {
				char ch = out[i];
				if (ch == '\r') {
					buf[pos++] = '\\';
					buf[pos++] = 'r';
				} else if (ch == '\n') {
					buf[pos++] = '\\';
					buf[pos++] = 'n';
				} else if (ch >= 32 && ch < 127) {
					buf[pos++] = ch;
				} else {
					pos += snprintf(buf + pos, sizeof(buf) - pos, "\\x%02X", (unsigned char)ch);
				}
			}
			buf[pos] = '\0';
			output(ctx, "  %s", buf);
		}
		return 0;
	}

	/* Process escape sequences and queue input */
	char processed[4096];
	size_t out_pos = 0;
	const char* p = args;

	while (*p && out_pos < sizeof(processed) - 1) {
		if (*p == '\\' && *(p + 1)) {
			p++;
			switch (*p) {
				case 'r': processed[out_pos++] = '\r'; break;
				case 'n': processed[out_pos++] = '\n'; break;
				case 't': processed[out_pos++] = '\t'; break;
				case '\\': processed[out_pos++] = '\\'; break;
				case '0': processed[out_pos++] = '\0'; break;
				default:
					/* Unknown escape - keep as-is */
					processed[out_pos++] = '\\';
					processed[out_pos++] = *p;
					break;
			}
			p++;
		} else {
			processed[out_pos++] = *p++;
		}
	}
	processed[out_pos] = '\0';

	/* Automatically append CR if not present - simulates pressing Enter */
	if (out_pos > 0 && processed[out_pos - 1] != '\r' && processed[out_pos - 1] != '\n') {
		if (out_pos < sizeof(processed) - 2) {
			processed[out_pos++] = '\r';
			processed[out_pos] = '\0';
		}
	}

	/* Append EOT (0x04 = Ctrl-D) to signal end of input.
	 * This prevents infinite loops in programs that keep calling DVINST
	 * expecting more input - they'll get EOT and treat it as end of file. */
	if (out_pos < sizeof(processed) - 1) {
		processed[out_pos++] = '\x04';  /* EOT = End of Transmission */
		processed[out_pos] = '\0';
	}

	/* Queue the processed input */
	mon_queue_console_input(processed);

	output(ctx, "Queued %zu chars: \"%s\"", out_pos, args);
	output(ctx, "Total pending: %zu chars", mon_get_console_input_remaining());
	return 0;
}
