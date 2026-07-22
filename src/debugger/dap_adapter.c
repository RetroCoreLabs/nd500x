/*
 * DAP (Debug Adapter Protocol) adapter for the nd500x emulator.
 *
 * Mirrors the nd100x integration: libdap owns the TCP transport, JSON
 * parsing and response serialization; this file implements the command
 * callbacks that bridge DAP requests to the nd500x machine/CPU API.
 *
 * Threading model:
 *  - The DAP server runs on its own pthread, pumping dap_server_run()
 *    every ~10ms (see dap_server_thread below).
 *  - Callbacks never execute the CPU directly. continue/step ask the
 *    machine to run (nd500_dbg_run spawns the CPU thread) or perform a
 *    bounded synchronous step (nd500_dbg_step).
 *  - The CPU signals stops through m->run_flag / m->stop_reason
 *    (set in cpu.c on breakpoints/traps and io.c on watchpoints).
 *    cmd_check_cpu_events(), called by libdap every loop iteration,
 *    polls the stop reason, sends the DAP 'stopped' event and clears it.
 *
 * Breakpoints/watchpoints reuse the existing BreakpointManager on
 * m->bp_mgr (the same engine the CLI 'bp'/'wp' commands drive).
 * DAP replace-all semantics are implemented by tracking the addresses
 * this adapter added and removing exactly those on each set request.
 *
 * Memory access uses nd500_dbg_mem_read_raw/write_raw which bypass the
 * bus layer, so debugger reads/writes never trigger watchpoints.
 * All DAP memory references are physical addresses (the same address
 * space the watchpoint engine observes).
 */
#ifdef DAP_ENABLED
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include "debugger.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"
#include "../cpu/cpu_protos.h"
#include "../cpu/instruction_helpers.h"
#include "../cpu/nd500_mmu.h"
#include "../disasm/nd500_disasm.h"
#include "../ndlib/ndlib.h"
#include <ndmon/mon_file_table.h>
#include "../../external/libdap/libdap/include/dap_server.h"
#include "../../external/libdap/libdap/include/dap_server_cmds.h"

/* ── Adapter state ─────────────────────────────────────────────── */

static Nd500Machine* g_machine = NULL;
static DAPServer* g_server = NULL;
static pthread_t g_dap_thread;
static volatile int g_dap_thread_running = 0;
static volatile int g_dap_thread_exit = 0;

/* Set after a synchronous step completed so cmd_check_cpu_events
 * reports a 'stopped' event with reason "step". */
static volatile int g_step_pending = 0;

/* Set by cmd_disconnect: the server thread recycles the transport
 * after the response goes out so a new client can attach. */
static volatile int g_recycle_transport = 0;

/* Entry PC captured at launch (for restart) */
static uint32_t g_entry_pc = 0;
static int g_have_entry_pc = 0;

/* Scope variablesReference IDs. The scope layout mirrors the CLI
 * 'regs' command (cmd_regs in commands.c), which is the canonical
 * ND-500 register report. */
#define SCOPE_ID_CORE         1001
#define SCOPE_ID_INTEGER      1002
#define SCOPE_ID_FLOAT        1003
#define SCOPE_ID_ADDRESSING   1004
#define SCOPE_ID_SPECIAL      1005
#define SCOPE_ID_TRAP_CONTROL 1006
#define SCOPE_ID_STATUS_FLAGS 1100
#define SCOPE_ID_MMU          1200

/* One register variable: name shown in the client, the shared
 * register-table name used for reads/writes, and a description
 * (taken from the CLI 'regs' output) surfaced as the DAP type. */
typedef struct {
	const char* name;
	const char* desc;
} DapRegDef;

static const DapRegDef g_core_regs[] = {
	{"PC",    "Program Counter (P register) - Current instruction address"},
	{"FLAGS", "Simplified status flags (emulator-internal; the real CPU status is ST1/ST2)"},
	{"ST1",   "Status Register ST bits 0:31 (data status, trap conditions)"},
	{"ST2",   "Status Register ST bits 32:63"},
};
static const DapRegDef g_integer_regs[] = {
	{"I1", "Integer register 1 (W1/H1/BY1/BI1)"},
	{"I2", "Integer register 2 (W2/H2/BY2/BI2)"},
	{"I3", "Integer register 3 (W3/H3/BY3/BI3)"},
	{"I4", "Integer register 4 (W4/H4/BY4/BI4)"},
};
static const DapRegDef g_float_regs[] = {
	{"A1", "Float accumulator 1 (F1 single, D1 low)"},
	{"A2", "Float accumulator 2 (F2 single, D2 low)"},
	{"A3", "Float accumulator 3 (F3 single, D3 low)"},
	{"A4", "Float accumulator 4 (F4 single, D4 low)"},
	{"E1", "Float extension 1 (D1 high 32 bits)"},
	{"E2", "Float extension 2 (D2 high 32 bits)"},
	{"E3", "Float extension 3 (D3 high 32 bits)"},
	{"E4", "Float extension 4 (D4 high 32 bits)"},
};
static const DapRegDef g_addressing_regs[] = {
	{"L", "Link register - Return address"},
	{"B", "Base register - Local frame pointer"},
	{"R", "Record register - Structure base pointer"},
};
static const DapRegDef g_special_regs[] = {
	{"TOS", "Top of Stack - Stack overflow limit"},
	{"LL",  "Low Limit - Memory lower bound"},
	{"HL",  "High Limit - Memory upper bound"},
	{"THA", "Trap Handler Address - Exception entry point"},
};
static const DapRegDef g_mmu_regs[] = {
	{"CED",     "Current Executing Domain"},
	{"CAD",     "Current Alternative Domain"},
	{"PS",      "Process Segment register"},
	{"PSTP",    "Physical Segment Table Pointer"},
	{"DITBASE", "Domain Information Table base address"},
};
static const DapRegDef g_trap_regs[] = {
	{"OTE1",  "Own Trap Enable (low 32)"},
	{"OTE2",  "Own Trap Enable (high 32)"},
	{"CTE1",  "Child Trap Enable (low 32)"},
	{"CTE2",  "Child Trap Enable (high 32)"},
	{"MTE1",  "Mother Trap Enable (low 32)"},
	{"MTE2",  "Mother Trap Enable (high 32)"},
	{"TEMM1", "Trap Enable Modification Mask (low 32)"},
	{"TEMM2", "Trap Enable Modification Mask (high 32)"},
};

#define DAP_REG_COUNT(a) ((int)(sizeof(a) / sizeof((a)[0])))

/* Tracking of DAP-owned breakpoints/watchpoints in the shared
 * BreakpointManager, so replace-all requests remove only what DAP added
 * (CLI-created entries are left alone). */
#define DAP_MAX_TRACKED 64
static uint32_t g_dap_src_bp_addrs[DAP_MAX_TRACKED];
static int g_dap_src_bp_count = 0;
static uint32_t g_dap_instr_bp_addrs[DAP_MAX_TRACKED];
static int g_dap_instr_bp_count = 0;
typedef struct { uint32_t addr; uint32_t len; WatchpointType type; } DapTrackedWp;
static DapTrackedWp g_dap_wps[DAP_MAX_TRACKED];
static int g_dap_wp_count = 0;

/* ── Helpers ───────────────────────────────────────────────────── */

static const char* stop_reason_to_dap(StopReason r) {
	switch (r) {
	case STOP_BREAKPOINT:        return "breakpoint";
	case STOP_WATCHPOINT_READ:     return "data breakpoint";
	case STOP_WATCHPOINT_WRITE:    return "data breakpoint";
	case STOP_WATCHPOINT_REGISTER: return "data breakpoint";
	case STOP_USER_REQUESTED:    return "pause";
	case STOP_NONE:              return "step";
	default:                     return "exception";
	}
}

/* Remove DAP-owned execution breakpoints from the manager.
 * Ids are array indexes that shift on delete, so scan from the top. */
static void dap_remove_tracked_bps(BreakpointManager* mgr, const uint32_t* addrs, int count) {
	if (!mgr) return;
	for (int i = mgr->bp_count - 1; i >= 0; i--) {
		for (int j = 0; j < count; j++) {
			if (mgr->breakpoints[i].address == addrs[j]) {
				bp_delete(mgr, i);
				break;
			}
		}
	}
}

static void dap_remove_tracked_wps(BreakpointManager* mgr) {
	if (!mgr) return;
	for (int i = mgr->wp_count - 1; i >= 0; i--) {
		for (int j = 0; j < g_dap_wp_count; j++) {
			if (mgr->watchpoints[i].address == g_dap_wps[j].addr &&
			    mgr->watchpoints[i].length == g_dap_wps[j].len &&
			    mgr->watchpoints[i].type == g_dap_wps[j].type) {
				wp_delete(mgr, i);
				break;
			}
		}
	}
	g_dap_wp_count = 0;
}

/* Translate a DAP memory reference to a physical address.
 * Aligned with nd100x: an explicit phys:/P: prefix means raw physical;
 * everything else is a virtual address translated through the MMU when
 * it is enabled (ispace uses the program page tables, dspace/virtual
 * the data page tables). With the MMU off, virtual == physical. */
static uint32_t dap_xlate_addr(uint32_t addr, DAPDataBreakpointAddressSpace aspace) {
	if (aspace == DAP_DATA_BP_ADDR_PHYSICAL) return addr;
	if (!g_machine || !g_machine->cpu) return addr;
	if (!nd500_mmu_is_enabled(g_machine->cpu)) return addr;
	int is_instruction = (aspace == DAP_DATA_BP_ADDR_ISPACE) ? 1 : 0;
	/* Translation may raise a CPU trap on protection/mapping errors.
	 * A debugger access must never leave a trap pending for the CPU
	 * thread to trip over, so clear any trap this call produced. */
	int trap_before = nd500_trap_occurred();
	uint32_t paddr = nd500_mmu_translate(g_machine->cpu, addr, 0, is_instruction);
	if (!trap_before && nd500_trap_occurred()) {
		nd500_trap_clear();
	}
	return paddr;
}

/* Standard base64 decoder (libdap only ships an encoder). */
static size_t dap_base64_decode(const char* in, uint8_t* out, size_t out_cap) {
	static const char tab[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	size_t in_len = in ? strlen(in) : 0;
	size_t out_len = 0;
	for (size_t i = 0; i + 3 < in_len; i += 4) {
		int v[4] = {0, 0, 0, 0};
		int valid = 0;
		for (int j = 0; j < 4; j++) {
			char c = in[i + j];
			if (c == '=') { v[j] = 0; }
			else {
				const char* p = strchr(tab, c);
				if (p) { v[j] = (int)(p - tab); valid++; }
			}
		}
		if (valid >= 2 && out_len < out_cap) out[out_len++] = (uint8_t)((v[0] << 2) | (v[1] >> 4));
		if (valid >= 3 && out_len < out_cap) out[out_len++] = (uint8_t)(((v[1] & 0xF) << 4) | (v[2] >> 2));
		if (valid >= 4 && out_len < out_cap) out[out_len++] = (uint8_t)(((v[2] & 0x3) << 6) | v[3]);
	}
	return out_len;
}

/* Format one disassembled instruction at addr into buf.
 * Returns the instruction length in bytes (minimum 1). */
static uint32_t dap_format_instruction(uint32_t addr, char* buf, size_t buf_size) {
	Nd500FetchedInstruction fi;
	memset(&fi, 0, sizeof(fi));
	int rc = nd500_decode_at(g_machine, addr, &fi);
	if (rc != 0 || fi.total_len == 0) {
		uint8_t b = 0;
		nd500_dbg_mem_read_raw(g_machine, addr, 1, &b, 1);
		snprintf(buf, buf_size, "??? ; 0x%02X", b);
		return 1;
	}
	char* p = buf;
	char* end = buf + buf_size;
	const char* mnem = fi.mnemonic ? fi.mnemonic : "???";
	if (nd500_instr_has_rn(fi.opcode)) {
		int dreg = nd500_instr_dest_reg(fi.opcode);
		if (dreg >= 0 && dreg < 4) {
			const char* dts = nd500_instr_dtype_prefix(fi.opcode);
			int n = snprintf(p, (size_t)(end - p), "%s%d ", dts, dreg + 1);
			if (n > 0) p += n;
		}
	}
	int n = snprintf(p, (size_t)(end - p), "%s", mnem);
	if (n > 0) p += n;
	for (uint8_t oi = 0; oi < fi.operand_count && p < end; ++oi) {
		char obuf[64];
		int ol = nd500_format_operand(obuf, sizeof(obuf), &fi.operands[oi], fi.data_type, false);
		if (ol > 0) {
			n = snprintf(p, (size_t)(end - p), "%s%s", (oi > 0) ? ", " : " ", obuf);
			if (n > 0) p += n;
		}
	}
	return fi.total_len;
}

/* ── Lifecycle callbacks ───────────────────────────────────────── */

static int cmd_wait_for_debugger(DAPServer* server) {
	(void)server;
	return 0;
}

static int cmd_release_debugger(DAPServer* server) {
	(void)server;
	return 0;
}

/* Stream captured console output to the client as it accumulates.
 * The queued console buffer only grows (or is reset by clear). */
static void dap_flush_console_output(DAPServer* server) {
	static size_t sent = 0;
	size_t len = mon_get_console_output_len();
	if (len < sent) sent = 0; /* buffer was cleared */
	if (len > sent) {
		const char* out = mon_get_console_output();
		dap_server_send_output_category(server, DAP_OUTPUT_STDOUT, out + sent);
		sent = len;
	}
}

/* Called by libdap on every server loop iteration. Polls the machine
 * stop state and sends 'stopped' events to the client. */
static int cmd_check_cpu_events(DAPServer* server) {
	if (!g_machine) return 0;

	dap_flush_console_output(server);

	if (g_machine->stop_reason != STOP_NONE && !g_machine->run_flag) {
		const char* reason = stop_reason_to_dap(g_machine->stop_reason);
		char desc[192];
		uint32_t pc = g_machine->cpu ? g_machine->cpu->PC : 0;
		const char* sym = ndlib_symbols_name_for_addr(pc);
		if (g_machine->stop_reason == STOP_WATCHPOINT_READ ||
		    g_machine->stop_reason == STOP_WATCHPOINT_WRITE) {
			snprintf(desc, sizeof(desc), "%s at PC=0x%08X data=0x%08X%s%s",
			         nd500_stop_reason_str(g_machine->stop_reason), pc,
			         g_machine->stop_addr,
			         (sym && *sym) ? " in " : "", (sym && *sym) ? sym : "");
		} else {
			snprintf(desc, sizeof(desc), "%s at PC=0x%08X%s%s",
			         nd500_stop_reason_str(g_machine->stop_reason), pc,
			         (sym && *sym) ? " in " : "", (sym && *sym) ? sym : "");
		}
		if (dap_server_send_stopped_event(server, reason, desc) == 0) {
			server->debugger_state.has_stopped = true;
			server->debugger_state.program_counter = (int)pc;
			g_machine->stop_reason = STOP_NONE;
			g_step_pending = 0;
			dap_server_send_output_category(server, DAP_OUTPUT_CONSOLE, desc);
		}
	} else if (g_step_pending && !g_machine->run_flag) {
		char desc[96];
		uint32_t pc = g_machine->cpu ? g_machine->cpu->PC : 0;
		snprintf(desc, sizeof(desc), "Stepped to PC=0x%08X", pc);
		if (dap_server_send_stopped_event(server, "step", desc) == 0) {
			server->debugger_state.has_stopped = true;
			server->debugger_state.program_counter = (int)pc;
			g_step_pending = 0;
		}
	}
	return 0;
}

/* ── Session callbacks ─────────────────────────────────────────── */

static int cmd_launch(DAPServer* server) {
	if (!g_machine) return -1;
	const char* program_path = server->debugger_state.program_path;
	bool stop_at_entry = server->debugger_state.stop_at_entry;

	if (program_path && *program_path) {
		uint32_t entry = 0, pc = 0;
		if (ndlib_load_aout_with_debug(g_machine, program_path, 1, &entry, &pc) != 0) {
			char msg[512];
			snprintf(msg, sizeof(msg), "Failed to load program: %s\n", program_path);
			dap_server_send_output_category(server, DAP_OUTPUT_STDERR, msg);
			return -1;
		}
		g_entry_pc = pc;
		g_have_entry_pc = 1;
		char msg[512];
		snprintf(msg, sizeof(msg), "Loaded %s (entry PC=0x%08X)\n", program_path, pc);
		dap_server_send_output_category(server, DAP_OUTPUT_CONSOLE, msg);
	} else if (g_machine->cpu) {
		/* No program: attach to the machine as prepared (e.g. DOM
		 * loaded via the REPL or --dom before starting DAP). */
		g_entry_pc = g_machine->cpu->PC;
		g_have_entry_pc = 1;
	}

	server->attached = true;
	server->is_running = true;
	server->debugger_state.current_thread_id = 1;
	server->debugger_state.program_counter = g_machine->cpu ? (int)g_machine->cpu->PC : 0;

	dap_server_send_process_event(server, program_path ? program_path : "nd500x", 1, true, "launch");

	if (stop_at_entry) {
		server->debugger_state.has_stopped = true;
		dap_server_send_stopped_event(server, "entry", "Stopped at program entry");
	} else {
		server->debugger_state.has_stopped = false;
		dap_server_send_thread_event(server, "started", 1);
		g_machine->stop_reason = STOP_NONE;
		nd500_dbg_run(g_machine);
	}
	return 0;
}

static int cmd_attach(DAPServer* server) {
	if (!g_machine) return -1;
	server->attached = true;
	server->is_running = true;
	server->debugger_state.current_thread_id = 1;
	server->debugger_state.has_stopped = !g_machine->run_flag;
	if (g_machine->cpu) {
		g_entry_pc = g_machine->cpu->PC;
		g_have_entry_pc = 1;
		server->debugger_state.program_counter = (int)g_machine->cpu->PC;
	}
	return 0;
}

static int cmd_configuration_done(DAPServer* server) {
	server->debugger_state.configuration_done = true;
	return 0;
}

static int cmd_disconnect(DAPServer* server) {
	if (g_machine && server->current_command.context.disconnect.terminate_debuggee) {
		nd500_dbg_stop(g_machine);
	}
	server->attached = false;
	/* Transport EOF detection is unreliable; explicitly recycle after
	 * the disconnect response is sent so a new client can connect. */
	g_recycle_transport = 1;
	return 0;
}

static int cmd_terminate(DAPServer* server) {
	if (g_machine) nd500_dbg_stop(g_machine);
	dap_server_send_terminated_event(server, false);
	return 0;
}

static int cmd_restart(DAPServer* server) {
	if (!g_machine || !g_machine->cpu) return -1;
	nd500_dbg_stop(g_machine);
	if (g_have_entry_pc) {
		g_machine->cpu->PC = g_entry_pc;
	}
	g_machine->stop_reason = STOP_NONE;
	server->debugger_state.has_stopped = true;
	dap_server_send_stopped_event(server, "entry", "Restarted at program entry");
	return 0;
}

/* ── Execution control ─────────────────────────────────────────── */

static int cmd_continue(DAPServer* server) {
	if (!g_machine) return -1;
	g_machine->stop_reason = STOP_NONE;
	g_step_pending = 0;
	server->debugger_state.has_stopped = false;
	server->current_command.context.continue_cmd.all_threads_continue = true;
	nd500_dbg_run(g_machine);
	return 0;
}

/* All step flavors are single-instruction steps: the ND-500 adapter has
 * no reliable source-line stepping yet, and step-out would need frame
 * unwinding. Executed synchronously (one instruction is fast). */
static int cmd_step_common(DAPServer* server) {
	if (!g_machine) return -1;
	if (g_machine->run_flag) return -1; /* already running */
	g_machine->stop_reason = STOP_NONE;
	server->debugger_state.has_stopped = false;
	nd500_dbg_step(g_machine, 1);
	if (g_machine->cpu) {
		server->debugger_state.program_counter = (int)g_machine->cpu->PC;
	}
	/* If the step tripped a watchpoint/trap, stop_reason is set and
	 * cmd_check_cpu_events reports it; otherwise report 'step'. */
	if (g_machine->stop_reason == STOP_NONE) {
		g_step_pending = 1;
	}
	return 0;
}

static int cmd_next(DAPServer* server)     { return cmd_step_common(server); }
static int cmd_step_in(DAPServer* server)  { return cmd_step_common(server); }
static int cmd_step_out(DAPServer* server) { return cmd_step_common(server); }

static int cmd_pause(DAPServer* server) {
	(void)server;
	if (!g_machine) return -1;
	nd500_dbg_stop(g_machine);
	/* libdap's pause handler sends the 'stopped' event itself;
	 * do not set a stop reason here or the client gets a duplicate. */
	return 0;
}

/* ── Breakpoints ───────────────────────────────────────────────── */

static int cmd_set_breakpoints(DAPServer* server) {
	if (!g_machine || !g_machine->bp_mgr) return -1;
	BreakpointCommandContext* ctx = &server->current_command.context.breakpoint;

	/* Replace-all semantics: remove the source breakpoints DAP added
	 * previously, then add the new set. */
	dap_remove_tracked_bps(g_machine->bp_mgr, g_dap_src_bp_addrs, g_dap_src_bp_count);
	g_dap_src_bp_count = 0;

	for (int i = 0; i < ctx->breakpoint_count; i++) {
		DAPBreakpoint* bp = &ctx->breakpoints[i];
		uint32_t addr = 0;
		int found = 0;
		if (ctx->source_path) {
			found = (ndlib_symbols_addr_for_line(ctx->source_path, bp->line, &addr) == 0);
			if (!found) {
				/* Retry with the basename (client may send full path) */
				const char* base = strrchr(ctx->source_path, '/');
				if (base) {
					found = (ndlib_symbols_addr_for_line(base + 1, bp->line, &addr) == 0);
				}
			}
		}
		if (!found) {
			bp->verified = false;
			bp->message = strdup("No address known for this source line");
			continue;
		}
		if (bp_add(g_machine->bp_mgr, addr, false) < 0) {
			bp->verified = false;
			bp->message = strdup("Breakpoint table full");
			continue;
		}
		bp->verified = true;
		bp->instruction_reference = addr;
		if (g_dap_src_bp_count < DAP_MAX_TRACKED) {
			g_dap_src_bp_addrs[g_dap_src_bp_count++] = addr;
		}
	}
	return 0;
}

static int cmd_set_instruction_breakpoints(DAPServer* server) {
	if (!g_machine || !g_machine->bp_mgr) return -1;
	InstructionBreakpointCommandContext* ctx =
		&server->current_command.context.instruction_breakpoint;

	dap_remove_tracked_bps(g_machine->bp_mgr, g_dap_instr_bp_addrs, g_dap_instr_bp_count);
	g_dap_instr_bp_count = 0;

	for (int i = 0; i < ctx->breakpoint_count; i++) {
		uint32_t addr = ctx->addresses[i] + (uint32_t)(ctx->offsets ? ctx->offsets[i] : 0);
		DAPBreakpoint* bp = &ctx->breakpoints[i];
		if (bp_add(g_machine->bp_mgr, addr, false) < 0) {
			bp->verified = false;
			bp->message = strdup("Breakpoint table full");
			continue;
		}
		bp->verified = true;
		bp->instruction_reference = addr;
		if (g_dap_instr_bp_count < DAP_MAX_TRACKED) {
			g_dap_instr_bp_addrs[g_dap_instr_bp_count++] = addr;
		}
	}
	return 0;
}

/* dataId contract between dataBreakpointInfo and setDataBreakpoints
 * (aligned with nd100x, which uses "V:"/"P:" space prefixes):
 * "V:0xADDRESS:LENGTH" - virtual address (translated via MMU at set time)
 * "P:0xADDRESS:LENGTH" - physical address (used as-is)
 * Address is hex, length is a decimal byte count. */
static int cmd_data_breakpoint_info(DAPServer* server) {
	DataBreakpointInfoCommandContext* ctx =
		&server->current_command.context.data_breakpoint_info;
	ctx->data_id = NULL;
	if (!g_machine || !ctx->name) {
		ctx->description = strdup("No expression given");
		return 0;
	}

	uint32_t addr = 0;
	int found = 0;
	const char* name = ctx->name;
	char space = 'V';

	/* Optional address-space prefix (nd100x convention) */
	if (strncmp(name, "phys:", 5) == 0)        { space = 'P'; name += 5; }
	else if (strncmp(name, "P:", 2) == 0)      { space = 'P'; name += 2; }
	else if (strncmp(name, "dspace:", 7) == 0) { space = 'V'; name += 7; }
	else if (strncmp(name, "D:", 2) == 0)      { space = 'V'; name += 2; }
	else if (strncmp(name, "ispace:", 7) == 0) { space = 'V'; name += 7; }
	else if (strncmp(name, "I:", 2) == 0)      { space = 'V'; name += 2; }
	else if (strncmp(name, "V:", 2) == 0)      { space = 'V'; name += 2; }

	/* Register watch: a watchable register name (PC, I1-I4, L, B, R)
	 * becomes an "R:<NAME>" dataId (break when the register changes). */
	if (!ctx->as_address && wp_register_index_for_name(name) >= 0) {
		char data_id[16];
		snprintf(data_id, sizeof(data_id), "R:%s", name);
		ctx->data_id = strdup(data_id);
		char desc[64];
		snprintf(desc, sizeof(desc), "Register %s (break on change)", name);
		ctx->description = strdup(desc);
		ctx->supports_read = false;
		ctx->supports_write = true;
		ctx->supports_read_write = false;
		ctx->can_persist = false;
		return 0;
	}

	/* Try symbol lookup first (unless explicitly an address) */
	if (!ctx->as_address) {
		uint8_t type = 0;
		found = (ndlib_symbols_lookup(name, &addr, &type) == 0);
	}
	if (!found) {
		char* endp = NULL;
		unsigned long v = strtoul(name, &endp, 0);
		if (endp && endp != name && (*endp == '\0' || *endp == ':')) {
			addr = (uint32_t)v;
			found = 1;
		}
	}
	if (!found) {
		char msg[192];
		snprintf(msg, sizeof(msg), "Cannot resolve '%s' to an address", ctx->name);
		ctx->description = strdup(msg);
		return 0;
	}

	int bytes = ctx->bytes > 0 ? ctx->bytes : 4;
	char data_id[48];
	snprintf(data_id, sizeof(data_id), "%c:0x%08X:%d", space, addr, bytes);
	ctx->data_id = strdup(data_id);
	char desc[96];
	snprintf(desc, sizeof(desc), "%d byte(s) at %s address 0x%08X",
	         bytes, space == 'P' ? "physical" : "virtual", addr);
	ctx->description = strdup(desc);
	ctx->supports_read = true;
	ctx->supports_write = true;
	ctx->supports_read_write = true;
	ctx->can_persist = false;
	return 0;
}

static int cmd_set_data_breakpoints(DAPServer* server) {
	if (!g_machine || !g_machine->bp_mgr) return -1;
	SetDataBreakpointsCommandContext* ctx =
		&server->current_command.context.set_data_breakpoints;

	/* Replace-all semantics for DAP-owned watchpoints */
	dap_remove_tracked_wps(g_machine->bp_mgr);

	for (int i = 0; i < ctx->breakpoint_count; i++) {
		DAPBreakpoint* bp = &ctx->breakpoints[i];
		const char* data_id = ctx->data_ids ? ctx->data_ids[i] : NULL;
		bp->verified = false;
		if (!data_id) {
			bp->message = strdup("Missing dataId");
			continue;
		}
		/* Register watch: "R:<NAME>" breaks when the register changes */
		if (data_id[0] == 'R' && data_id[1] == ':') {
			const char* reg_name = data_id + 2;
			int reg_index = wp_register_index_for_name(reg_name);
			if (reg_index < 0) {
				bp->message = strdup("Unknown register in dataId");
				continue;
			}
			int wid = wp_add_register(g_machine->bp_mgr, reg_name, (uint32_t)reg_index);
			if (wid < 0) {
				bp->message = strdup("Watchpoint table full");
				continue;
			}
			if (g_machine->cpu) {
				uint32_t cur[WP_REG_INDEX_COUNT] = {
					g_machine->cpu->PC, g_machine->cpu->I[0],
					g_machine->cpu->I[1], g_machine->cpu->I[2],
					g_machine->cpu->I[3], g_machine->cpu->L,
					g_machine->cpu->B, g_machine->cpu->R
				};
				g_machine->bp_mgr->watchpoints[wid].last_value = cur[reg_index];
			}
			if (g_dap_wp_count < DAP_MAX_TRACKED) {
				g_dap_wps[g_dap_wp_count].addr = (uint32_t)reg_index;
				g_dap_wps[g_dap_wp_count].len = 4;
				g_dap_wps[g_dap_wp_count].type = WP_TYPE_REGISTER;
				g_dap_wp_count++;
			}
			bp->verified = true;
			continue;
		}

		/* Parse "V:0xADDR:LEN" / "P:0xADDR:LEN" (bare "0xADDR" = virtual) */
		const char* p = data_id;
		char space = 'V';
		if ((p[0] == 'V' || p[0] == 'P') && p[1] == ':') {
			space = p[0];
			p += 2;
		}
		char* endp = NULL;
		uint32_t addr = (uint32_t)strtoul(p, &endp, 0);
		uint32_t len = 4;
		if (endp && *endp == ':') {
			len = (uint32_t)strtoul(endp + 1, NULL, 10);
			if (len == 0) len = 4;
		}
		/* The watchpoint engine (io.c) observes physical bus addresses:
		 * translate virtual watch addresses through the MMU at set time. */
		if (space == 'V') {
			addr = dap_xlate_addr(addr, DAP_DATA_BP_ADDR_VIRTUAL);
		}

		const char* access = (ctx->access_types && ctx->access_types[i])
			? ctx->access_types[i] : "write";
		int want_read = (strcmp(access, "read") == 0 || strcmp(access, "readWrite") == 0);
		int want_write = (strcmp(access, "write") == 0 || strcmp(access, "readWrite") == 0);

		int ok = 1;
		if (want_read) {
			if (wp_add(g_machine->bp_mgr, addr, len, WP_TYPE_READ) < 0) ok = 0;
			else if (g_dap_wp_count < DAP_MAX_TRACKED) {
				g_dap_wps[g_dap_wp_count].addr = addr;
				g_dap_wps[g_dap_wp_count].len = len;
				g_dap_wps[g_dap_wp_count].type = WP_TYPE_READ;
				g_dap_wp_count++;
			}
		}
		if (ok && want_write) {
			if (wp_add(g_machine->bp_mgr, addr, len, WP_TYPE_WRITE) < 0) ok = 0;
			else if (g_dap_wp_count < DAP_MAX_TRACKED) {
				g_dap_wps[g_dap_wp_count].addr = addr;
				g_dap_wps[g_dap_wp_count].len = len;
				g_dap_wps[g_dap_wp_count].type = WP_TYPE_WRITE;
				g_dap_wp_count++;
			}
		}
		if (!ok) {
			bp->message = strdup("Watchpoint table full");
			continue;
		}
		bp->verified = true;
		bp->instruction_reference = addr;
	}
	return 0;
}

/* ── Inspection ────────────────────────────────────────────────── */

static int cmd_stack_trace(DAPServer* server) {
	StackTraceCommandContext* ctx = &server->current_command.context.stack_trace;
	ctx->frames = NULL;
	ctx->frame_count = 0;
	ctx->total_frames = 0;
	if (!g_machine || !g_machine->cpu) return -1;

	/* Single frame: the ND-500 has no standard frame chain the adapter
	 * can walk yet; report the current PC with symbol and source info. */
	ctx->frames = (DAPStackFrame*)calloc(1, sizeof(DAPStackFrame));
	if (!ctx->frames) return -1;

	uint32_t pc = g_machine->cpu->PC;
	DAPStackFrame* f = &ctx->frames[0];
	f->id = 0;
	const char* sym = ndlib_symbols_name_for_addr(pc);
	if (sym && *sym) {
		f->name = strdup(sym);
		f->valid_symbol = true;
	} else {
		char buf[32];
		snprintf(buf, sizeof(buf), "0x%08X", pc);
		f->name = strdup(buf);
		f->valid_symbol = false;
	}
	f->instruction_pointer_reference = (int)pc;

	const char* file = NULL;
	int line = 0;
	if (ndlib_symbols_get_c_mapping(pc, &file, &line) == 0 && file && line > 0) {
		f->source_path = strdup(file);
		const char* base = strrchr(file, '/');
		f->source_name = strdup(base ? base + 1 : file);
		f->line = line;
		f->column = 1;
	}

	ctx->frame_count = 1;
	ctx->total_frames = 1;
	return 0;
}

static int cmd_scopes(DAPServer* server) {
	ScopesCommandContext* ctx = &server->current_command.context.scopes;
	static const struct { const char* name; int ref; int count; } defs[] = {
		{"Core",              SCOPE_ID_CORE,         DAP_REG_COUNT(g_core_regs) + 1},
		{"Integer Registers", SCOPE_ID_INTEGER,      DAP_REG_COUNT(g_integer_regs)},
		{"Float Registers",   SCOPE_ID_FLOAT,        DAP_REG_COUNT(g_float_regs) + 4},
		{"Addressing",        SCOPE_ID_ADDRESSING,   DAP_REG_COUNT(g_addressing_regs)},
		{"Special",           SCOPE_ID_SPECIAL,      DAP_REG_COUNT(g_special_regs)},
		{"MMU / Domain",      SCOPE_ID_MMU,          DAP_REG_COUNT(g_mmu_regs)},
		{"Trap Control",      SCOPE_ID_TRAP_CONTROL, DAP_REG_COUNT(g_trap_regs)},
		{"Status Flags",      SCOPE_ID_STATUS_FLAGS, 7},
	};
	const int NUM_SCOPES = (int)(sizeof(defs) / sizeof(defs[0]));
	DAPScope* scopes = (DAPScope*)calloc((size_t)NUM_SCOPES, sizeof(DAPScope));
	if (!scopes) return -1;
	for (int i = 0; i < NUM_SCOPES; i++) {
		scopes[i].name = strdup(defs[i].name);
		scopes[i].variables_reference = defs[i].ref;
		scopes[i].named_variables = defs[i].count;
		scopes[i].expensive = false;
	}
	ctx->scopes = scopes;
	ctx->scope_count = NUM_SCOPES;
	return 0;
}

static DAPVariable* dap_add_variable(DAPServer* server, const char* name,
                                     const char* value, const char* type) {
	VariablesCommandContext* ctx = &server->current_command.context.variables;
	DAPVariable* arr = (DAPVariable*)realloc(ctx->variable_array,
		(size_t)(ctx->variable_count + 1) * sizeof(DAPVariable));
	if (!arr) return NULL;
	ctx->variable_array = arr;
	DAPVariable* var = &arr[ctx->variable_count++];
	memset(var, 0, sizeof(*var));
	var->name = strdup(name);
	var->value = strdup(value);
	var->type = strdup(type);
	return var;
}

static void dap_add_reg_group(DAPServer* server, const DapRegDef* defs, int count) {
	char value[48];
	for (int i = 0; i < count; i++) {
		uint32_t v = 0;
		nd500_dbg_reg_get_by_name(g_machine->cpu, defs[i].name, &v);
		snprintf(value, sizeof(value), "0x%08X", v);
		dap_add_variable(server, defs[i].name, value, defs[i].desc);
	}
}

static int cmd_variables(DAPServer* server) {
	VariablesCommandContext* ctx = &server->current_command.context.variables;
	ctx->variable_array = NULL;
	ctx->variable_count = 0;
	if (!g_machine || !g_machine->cpu) return -1;

	Nd500Cpu* cpu = g_machine->cpu;
	char value[64];

	switch (ctx->variables_reference) {
	case SCOPE_ID_CORE: {
		dap_add_reg_group(server, g_core_regs, DAP_REG_COUNT(g_core_regs));
		/* Flags ASCII summary (uppercase=set), as in the CLI 'regs' */
		char fa[8];
		fa[0] = (cpu->ST1 & ND500_FLAG_PIA) ? 'P' : 'p';
		fa[1] = (cpu->ST1 & ND500_FLAG_PSD) ? 'D' : 'd';
		fa[2] = (cpu->ST1 & ND500_FLAG_Z) ? 'Z' : 'z';
		fa[3] = (cpu->ST1 & ND500_FLAG_S) ? 'S' : 's';
		fa[4] = (cpu->ST1 & ND500_FLAG_C) ? 'C' : 'c';
		fa[5] = (cpu->ST1 & ND500_FLAG_K) ? 'K' : 'k';
		fa[6] = (cpu->ST1 & ND500_FLAG_O) ? 'O' : 'o';
		fa[7] = '\0';
		DAPVariable* var = dap_add_variable(server, "Flags", fa,
			"CPU Status Flags as ASCII (uppercase=set, lowercase=clear)");
		if (var) var->variables_reference = SCOPE_ID_STATUS_FLAGS;
		break;
	}
	case SCOPE_ID_INTEGER:
		dap_add_reg_group(server, g_integer_regs, DAP_REG_COUNT(g_integer_regs));
		break;
	case SCOPE_ID_FLOAT: {
		dap_add_reg_group(server, g_float_regs, DAP_REG_COUNT(g_float_regs));
		/* Computed 64-bit doubles D1-D4 (E:A), converted to IEEE754 */
		for (int i = 0; i < 4; i++) {
			uint64_t bits = ((uint64_t)cpu->E[i] << 32) | cpu->A[i];
			char name[4];
			snprintf(name, sizeof(name), "D%d", i + 1);
			snprintf(value, sizeof(value), "%f", nd500_double_to_ieee754(bits));
			dap_add_variable(server, name, value,
			                 "Double register (64-bit = E:A, read-only)");
		}
		break;
	}
	case SCOPE_ID_ADDRESSING:
		dap_add_reg_group(server, g_addressing_regs, DAP_REG_COUNT(g_addressing_regs));
		break;
	case SCOPE_ID_SPECIAL:
		dap_add_reg_group(server, g_special_regs, DAP_REG_COUNT(g_special_regs));
		break;
	case SCOPE_ID_MMU:
		dap_add_reg_group(server, g_mmu_regs, DAP_REG_COUNT(g_mmu_regs));
		break;
	case SCOPE_ID_TRAP_CONTROL:
		dap_add_reg_group(server, g_trap_regs, DAP_REG_COUNT(g_trap_regs));
		break;
	case SCOPE_ID_STATUS_FLAGS: {
		uint32_t st1 = cpu->ST1;
		/* ST1 data status bits per ND-05.009.4 Table 10:
		 * PIA=1, PSD=4, Z=5, C=6, S=7, K=8, O=9 */
		static const struct { const char* name; uint32_t mask; } flags[] = {
			{"PIA (Privileged instruction allowed, bit 1)", ND500_FLAG_PIA},
			{"PSD (Process switch disable, bit 4)",         ND500_FLAG_PSD},
			{"Z (Zero, bit 5)",                             ND500_FLAG_Z},
			{"C (Carry, bit 6)",                            ND500_FLAG_C},
			{"S (Sign, bit 7)",                             ND500_FLAG_S},
			{"K (Flag, bit 8)",                             ND500_FLAG_K},
			{"O (Overflow, bit 9)",                         ND500_FLAG_O},
		};
		for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); i++) {
			dap_add_variable(server, flags[i].name,
			                 (st1 & flags[i].mask) ? "true" : "false", "flag");
		}
		break;
	}
	default:
		break;
	}
	return 0;
}

static int cmd_set_variable(DAPServer* server) {
	SetVariableCommandContext* ctx = &server->current_command.context.set_variable;
	if (!g_machine || !g_machine->cpu || !ctx->name || !ctx->value) return -1;
	/* All register scopes are writable; computed variables (Flags,
	 * D1-D4) are rejected below because they have no table entry. */
	int ref = ctx->variables_reference;
	if (!((ref >= SCOPE_ID_CORE && ref <= SCOPE_ID_TRAP_CONTROL) ||
	      ref == SCOPE_ID_MMU)) {
		return -1;
	}

	/* Variables are shown with their raw table names */
	uint32_t value = (uint32_t)strtoul(ctx->value, NULL, 0);
	if (nd500_dbg_reg_set_by_name(g_machine->cpu, ctx->name, value) != 0) {
		return -1;
	}
	char buf[32];
	snprintf(buf, sizeof(buf), "0x%08X", value);
	ctx->new_value = strdup(buf);
	ctx->type = strdup("register");
	return 0;
}

static int cmd_evaluate(DAPServer* server) {
	EvaluateCommandContext* ctx = &server->current_command.context.evaluate;
	ctx->result = NULL;
	if (!g_machine || !g_machine->cpu || !ctx->expression) return -1;

	const char* expr = ctx->expression;
	uint32_t value = 0;
	int found = 0;

	/* 1. Register name */
	if (nd500_dbg_reg_get_by_name(g_machine->cpu, expr, &value) == 0) {
		found = 1;
	}
	/* 2. Symbol name */
	if (!found) {
		uint8_t type = 0;
		if (ndlib_symbols_lookup(expr, &value, &type) == 0) found = 1;
	}
	/* 3. Numeric literal (0x.., octal, decimal) */
	if (!found) {
		char* endp = NULL;
		unsigned long v = strtoul(expr, &endp, 0);
		if (endp && endp != expr && *endp == '\0') {
			value = (uint32_t)v;
			found = 1;
		}
	}
	if (!found) {
		char msg[160];
		snprintf(msg, sizeof(msg), "Cannot evaluate '%s'", expr);
		ctx->result = strdup(msg);
		ctx->type = strdup("error");
		return 0;
	}
	char buf[64];
	snprintf(buf, sizeof(buf), "0x%08X (%u)", value, value);
	ctx->result = strdup(buf);
	ctx->type = strdup("integer");
	ctx->memory_reference = value;
	return 0;
}

/* ── Memory ────────────────────────────────────────────────────── */

static int cmd_read_memory(DAPServer* server) {
	ReadMemoryCommandContext* ctx = &server->current_command.context.read_memory;
	ctx->base64_data = NULL;
	if (!g_machine) return -1;

	uint32_t addr = ctx->memory_reference + (uint32_t)ctx->offset;
	size_t count = ctx->count > 0 ? (size_t)ctx->count : 0;
	ctx->unreadable_bytes = count;
	if (count == 0) {
		ctx->base64_data = base64_encode((const uint8_t*)"", 0);
		ctx->unreadable_bytes = 0;
		return 0;
	}
	if (count > 65536) count = 65536;

	uint8_t* data = (uint8_t*)calloc(1, count);
	if (!data) return -1;
	/* Translate page by page (2KB pages); addresses within one page are
	 * physically contiguous. */
	size_t got = 0;
	size_t pos = 0;
	while (pos < count) {
		uint32_t vaddr = addr + (uint32_t)pos;
		uint32_t page_left = 0x800 - (vaddr & 0x7FF);
		uint32_t chunk = (uint32_t)(count - pos) < page_left ? (uint32_t)(count - pos) : page_left;
		uint32_t paddr = dap_xlate_addr(vaddr, ctx->address_space);
		got += nd500_dbg_mem_read_raw(g_machine, paddr, chunk, data + pos, chunk);
		pos += chunk;
	}
	ctx->unreadable_bytes = count - got;
	ctx->base64_data = base64_encode(data, count);
	free(data);
	return ctx->base64_data ? 0 : -1;
}

static int cmd_write_memory(DAPServer* server) {
	WriteMemoryCommandContext* ctx = &server->current_command.context.write_memory;
	ctx->bytes_written = 0;
	if (!g_machine || !ctx->data) return -1;

	uint32_t addr = ctx->memory_reference + (uint32_t)ctx->offset;
	size_t in_len = strlen(ctx->data);
	uint8_t* buf = (uint8_t*)malloc(in_len ? in_len : 1);
	if (!buf) return -1;
	size_t decoded = dap_base64_decode(ctx->data, buf, in_len ? in_len : 1);
	size_t written = 0;
	size_t pos = 0;
	while (pos < decoded) {
		uint32_t vaddr = addr + (uint32_t)pos;
		uint32_t page_left = 0x800 - (vaddr & 0x7FF);
		uint32_t chunk = (uint32_t)(decoded - pos) < page_left ? (uint32_t)(decoded - pos) : page_left;
		uint32_t paddr = dap_xlate_addr(vaddr, ctx->address_space);
		written += nd500_dbg_mem_write_raw(g_machine, paddr, buf + pos, chunk);
		pos += chunk;
	}
	free(buf);
	if (written < decoded && !ctx->allow_partial) {
		return -1;
	}
	ctx->bytes_written = (uint16_t)written;
	return 0;
}

/* ── Disassembly and symbols ───────────────────────────────────── */

static int cmd_disassemble(DAPServer* server) {
	DisassembleCommandContext* ctx = &server->current_command.context.disassemble;
	ctx->instructions = NULL;
	ctx->actual_instruction_count = 0;
	if (!g_machine) return -1;

	int requested = ctx->instruction_count > 0 ? ctx->instruction_count : 10;
	ctx->instructions = (DisassembleInstruction*)calloc((size_t)requested,
	                                                    sizeof(DisassembleInstruction));
	if (!ctx->instructions) return -1;

	uint32_t addr = ctx->memory_reference + (uint32_t)ctx->offset;

	/* instruction_offset: negative values would require backwards
	 * disassembly which is ambiguous on a variable-length ISA; skip
	 * forward for positive offsets only. */
	for (int skip = 0; skip < ctx->instruction_offset; skip++) {
		char tmp[8];
		addr += dap_format_instruction(addr, tmp, sizeof(tmp));
	}

	/* Note: addr is a virtual address; nd500_decode_at translates
	 * through the MMU internally, so no physical bounds check here. */
	int count = 0;
	for (int i = 0; i < requested; i++) {
		char inst_buf[128];
		uint32_t len = dap_format_instruction(addr, inst_buf, sizeof(inst_buf));

		char addr_str[16];
		snprintf(addr_str, sizeof(addr_str), "0x%08X", addr);
		ctx->instructions[count].address = strdup(addr_str);
		ctx->instructions[count].instruction = strdup(inst_buf);
		if (ctx->resolve_symbols) {
			const char* sym = ndlib_symbols_name_for_addr(addr);
			ctx->instructions[count].symbol = (sym && *sym) ? strdup(sym) : NULL;
		}
		count++;
		addr += len;
	}
	ctx->actual_instruction_count = count;
	return 0;
}

static int cmd_symbol_list(DAPServer* server) {
	SymbolListContext* ctx = &server->current_command.context.symbol_list;
	ctx->symbols = NULL;
	ctx->symbol_count = 0;

	int count = ndlib_symbols_get_count();
	if (count <= 0) return 0;

	ctx->symbols = (DAPSymbol*)calloc((size_t)count, sizeof(DAPSymbol));
	if (!ctx->symbols) return -1;

	int out = 0;
	for (int i = 0; i < count; i++) {
		const char* name = ndlib_symbols_get_name(i);
		if (!name || !*name) continue;
		ctx->symbols[out].name = strdup(name);
		ctx->symbols[out].address = ndlib_symbols_get_addr(i);
		ctx->symbols[out].type = strdup("label");
		out++;
	}
	ctx->symbol_count = out;
	return 0;
}

/* ── Console I/O (custom DAP commands, nd100x convention) ──────── */

static int cmd_console_enable(DAPServer* server) {
	ConsoleEnableContext* ctx = &server->current_command.context.console_enable;
	if (ctx->enable) {
		/* Queueing an empty string switches the MON console to the
		 * captured queue implementation without adding input. */
		mon_queue_console_input("");
	}
	return 0;
}

static int cmd_console_write(DAPServer* server) {
	ConsoleWriteContext* ctx = &server->current_command.context.console_write;
	if (!ctx->input) return -1;
	if (ctx->hex) {
		size_t in_len = strlen(ctx->input);
		char* buf = (char*)malloc(in_len / 2 + 1);
		if (!buf) return -1;
		size_t out = 0;
		for (size_t i = 0; i + 1 < in_len; i += 2) {
			unsigned int v = 0;
			if (sscanf(ctx->input + i, "%02x", &v) != 1) break;
			buf[out++] = (char)v;
		}
		buf[out] = '\0';
		mon_queue_console_input(buf);
		free(buf);
	} else {
		/* Translate backslash escapes: \r \n \t \\ (nd100x convention) */
		size_t in_len = strlen(ctx->input);
		char* buf = (char*)malloc(in_len + 1);
		if (!buf) return -1;
		size_t out = 0;
		for (size_t i = 0; i < in_len; i++) {
			char c = ctx->input[i];
			if (c == '\\' && i + 1 < in_len) {
				char e = ctx->input[++i];
				if (e == 'r') c = '\r';
				else if (e == 'n') c = '\n';
				else if (e == 't') c = '\t';
				else if (e == '\\') c = '\\';
				else { buf[out++] = c; c = e; }
			}
			buf[out++] = c;
		}
		buf[out] = '\0';
		mon_queue_console_input(buf);
		free(buf);
	}
	return 0;
}

/* ── Server setup / thread ─────────────────────────────────────── */

static int dap_register_callbacks(DAPServer* server) {
	dap_server_register_command_callback(server, DAP_CMD_LAUNCH, cmd_launch);
	dap_server_register_command_callback(server, DAP_CMD_ATTACH, cmd_attach);
	dap_server_register_command_callback(server, DAP_CMD_CONFIGURATION_DONE, cmd_configuration_done);
	dap_server_register_command_callback(server, DAP_CMD_DISCONNECT, cmd_disconnect);
	dap_server_register_command_callback(server, DAP_CMD_TERMINATE, cmd_terminate);
	dap_server_register_command_callback(server, DAP_CMD_RESTART, cmd_restart);

	dap_server_register_command_callback(server, DAP_CMD_CONTINUE, cmd_continue);
	dap_server_register_command_callback(server, DAP_CMD_NEXT, cmd_next);
	dap_server_register_command_callback(server, DAP_CMD_STEP_IN, cmd_step_in);
	dap_server_register_command_callback(server, DAP_CMD_STEP_OUT, cmd_step_out);
	dap_server_register_command_callback(server, DAP_CMD_PAUSE, cmd_pause);

	dap_server_register_command_callback(server, DAP_CMD_SET_BREAKPOINTS, cmd_set_breakpoints);
	dap_server_register_command_callback(server, DAP_CMD_SET_INSTRUCTION_BREAKPOINTS, cmd_set_instruction_breakpoints);
	dap_server_register_command_callback(server, DAP_CMD_DATA_BREAKPOINT_INFO, cmd_data_breakpoint_info);
	dap_server_register_command_callback(server, DAP_CMD_SET_DATA_BREAKPOINTS, cmd_set_data_breakpoints);

	dap_server_register_command_callback(server, DAP_CMD_STACK_TRACE, cmd_stack_trace);
	dap_server_register_command_callback(server, DAP_CMD_SCOPES, cmd_scopes);
	dap_server_register_command_callback(server, DAP_CMD_VARIABLES, cmd_variables);
	dap_server_register_command_callback(server, DAP_CMD_SET_VARIABLE, cmd_set_variable);
	dap_server_register_command_callback(server, DAP_CMD_EVALUATE, cmd_evaluate);

	dap_server_register_command_callback(server, DAP_CMD_READ_MEMORY, cmd_read_memory);
	dap_server_register_command_callback(server, DAP_CMD_WRITE_MEMORY, cmd_write_memory);
	dap_server_register_command_callback(server, DAP_CMD_DISASSEMBLE, cmd_disassemble);
	dap_server_register_command_callback(server, DAP_CMD_SYMBOL_LIST, cmd_symbol_list);
	dap_server_register_command_callback(server, DAP_CMD_CONSOLE_ENABLE, cmd_console_enable);
	dap_server_register_command_callback(server, DAP_CMD_CONSOLE_WRITE, cmd_console_write);

	dap_server_register_command_callback(server, DAP_WAIT_FOR_DEBUGGER, cmd_wait_for_debugger);
	dap_server_register_command_callback(server, DAP_RELEASE_DEBUGGER, cmd_release_debugger);
	dap_server_register_command_callback(server, DAP_CHECK_CPU_EVENTS, cmd_check_cpu_events);
	return 0;
}

static void dap_set_capabilities(void) {
	dap_server_set_capability(DAP_CAP_CONFIG_DONE_REQUEST, true);
	dap_server_set_capability(DAP_CAP_RESTART_REQUEST, true);
	dap_server_set_capability(DAP_CAP_TERMINATE_REQUEST, true);
	dap_server_set_capability(DAP_CAP_TERMINATE_DEBUGGEE, true);
	dap_server_set_capability(DAP_CAP_READ_MEMORY_REQUEST, true);
	dap_server_set_capability(DAP_CAP_WRITE_MEMORY_REQUEST, true);
	dap_server_set_capability(DAP_CAP_DISASSEMBLE_REQUEST, true);
	dap_server_set_capability(DAP_CAP_INSTRUCTION_BREAKPOINTS, true);
	dap_server_set_capability(DAP_CAP_DATA_BREAKPOINTS, true);
	dap_server_set_capability(DAP_CAP_SET_VARIABLE, true);
	dap_server_set_capability(DAP_CAP_EVALUATE_FOR_HOVERS, true);
	dap_server_set_capability(DAP_CAP_STEPPING_GRANULARITY, true);
}

static void* dap_server_thread(void* arg) {
	(void)arg;
	while (g_server && !g_dap_thread_exit) {
		if (g_recycle_transport || !g_server->is_running) {
			/* Client disconnected (or asked to): recycle the
			 * transport so a new client can attach to the same
			 * emulator session. */
			g_recycle_transport = 0;
			dap_server_stop(g_server);
			g_server->attached = false;
			if (dap_server_start(g_server) != 0) {
				fprintf(stderr, "DAP server restart failed\n");
				break;
			}
			printf("DAP server ready for new client\n");
		}
		if (dap_server_run(g_server) != 0) {
			fprintf(stderr, "DAP server loop failed\n");
			break;
		}
		struct timespec ts = {0, 10000000}; /* 10ms */
		nanosleep(&ts, NULL);
	}
	g_dap_thread_running = 0;
	return NULL;
}

/* Bind a machine and register all callbacks on a server without
 * starting the transport or server thread. Used by nd500_dap_start
 * and directly by the unit tests. */
int nd500_dap_bind(Nd500Machine* m, DAPServer* server) {
	if (!server) return -1;
	g_machine = m;
	g_dap_src_bp_count = 0;
	g_dap_instr_bp_count = 0;
	g_dap_wp_count = 0;
	g_step_pending = 0;
	return dap_register_callbacks(server);
}

int nd500_dap_start(Nd500Machine* m, int port) {
	if (g_dap_thread_running) {
		printf("DAP server already running\n");
		return -1;
	}

	DAPServerConfig config;
	memset(&config, 0, sizeof(config));
	config.transport.type = DAP_TRANSPORT_TCP;
	config.transport.config.tcp.host = "localhost";
	config.transport.config.tcp.port = port;

	g_server = dap_server_create(&config);
	if (!g_server) return -1;

	nd500_dap_bind(m, g_server);
	dap_set_capabilities();

	if (dap_server_start(g_server) != 0) {
		dap_server_free(g_server);
		g_server = NULL;
		return -1;
	}

	g_dap_thread_exit = 0;
	g_dap_thread_running = 1;
	if (pthread_create(&g_dap_thread, NULL, dap_server_thread, NULL) != 0) {
		g_dap_thread_running = 0;
		dap_server_stop(g_server);
		dap_server_free(g_server);
		g_server = NULL;
		return -1;
	}
	return 0;
}

int nd500_dap_stop(void) {
	if (!g_dap_thread_running || !g_server) return -1;
	g_dap_thread_exit = 1;
	pthread_join(g_dap_thread, NULL);
	dap_server_stop(g_server);
	dap_server_free(g_server);
	g_server = NULL;
	return 0;
}

int nd500_dap_is_active(void) {
	return g_dap_thread_running;
}

#endif /* DAP_ENABLED */
