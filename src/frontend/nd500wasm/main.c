#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../machine/machine_protos.h"
#include "../../cpu/cpu_protos.h"
#include "../../machine/breakpoints.h"
#include "../../ndlib/ndlib.h"
#include "../../disasm/nd500_disasm.h"
#include "../../debugger/commands.h"
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#include "../../cpu/cpu_protos.h"
#ifndef HAVE_SYSTEM_CJSON
#include <cjson/cJSON.h>
#endif

static Nd500Machine g_machine;
static Nd500Cpu g_cpu;

/* Anchor the generated instruction table in WASM to prevent dead-stripping */
extern const unsigned int g_nd500_instrs_count;
extern const struct Nd500InstrDef { unsigned short opcode; const char* mnemonic; unsigned char operands; unsigned char prefixes_mask; unsigned char variant; unsigned int op_templates[4]; } g_nd500_instrs[];
static unsigned int anchor_instr_table(void) {
    /* Read a couple of fields so the linker keeps the table */
    unsigned int n = g_nd500_instrs_count;
    unsigned int acc = n;
    if (n > 0) {
        acc ^= (unsigned int)g_nd500_instrs[0].opcode;
    }
    return acc;
}
/* Diagnostics: expose instruction table info to JS */
unsigned int nd500_dbg_instr_count_js(void) { return g_nd500_instrs_count; }
const char* nd500_dbg_mnemonic_js(unsigned int opcode) { return nd500_instr_mnemonic((uint16_t)opcode); }

void nd500wasm_init(void) {
	nd500_machine_init(&g_machine, 16 * 1024 * 1024);
	nd500_cpu_init(&g_cpu, &g_machine);
	nd500_cpu_reset(&g_cpu);
    /* Force reference to instruction table so it is linked in */
    (void)anchor_instr_table();
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
const char* nd500_dbg_build_info_js(void) {
    return "Built: " __DATE__ " " __TIME__;
}

EMSCRIPTEN_KEEPALIVE
void nd500_dbg_clear_symbols_js(void) {
    ndlib_symbols_clear();
}
#endif

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
int nd500_dbg_set_pc_js(uint32_t pc) {
    g_cpu.PC = pc;
    return 0;
}

EMSCRIPTEN_KEEPALIVE
int nd500_dbg_load_pseg_path_js(const char* path, uint32_t base_addr) {
    return nd500_load_pseg_file(&g_machine, path, base_addr);
}

EMSCRIPTEN_KEEPALIVE
int nd500_dbg_load_dseg_path_js(const char* path, uint32_t base_addr) {
    return nd500_load_dseg_file(&g_machine, path, base_addr);
}

EMSCRIPTEN_KEEPALIVE
const char* nd500_dbg_load_strerror_js(int error_code, uint32_t attempted_addr) {
    return nd500_load_strerror(error_code, attempted_addr, g_machine.memory_size);
}

EMSCRIPTEN_KEEPALIVE
uint32_t nd500_dbg_get_memory_size_js(void) {
    return g_machine.memory_size;
}

EMSCRIPTEN_KEEPALIVE
void nd500_dbg_reset_memory_js(void) {
    memset(g_machine.memory, 0, g_machine.memory_size);
    nd500_cpu_reset(&g_cpu);
}

EMSCRIPTEN_KEEPALIVE
int nd500_dbg_load_segments_path_js(const char* pseg_path, uint32_t pseg_base,
                                    const char* dseg_path, uint32_t dseg_base,
                                    int set_pc, uint32_t pc) {
    int rc = 0;
    if (pseg_path && *pseg_path) {
        rc = nd500_load_pseg_file(&g_machine, pseg_path, pseg_base);
        if (rc != 0) return rc;
    }
    if (dseg_path && *dseg_path) {
        rc = nd500_load_dseg_file(&g_machine, dseg_path, dseg_base);
        if (rc != 0) return rc;
    }
    if (set_pc) {
        g_cpu.PC = pc;
    }
    return 0;
}
#endif

static char* dup_json_string(cJSON* obj) {
	char* s = cJSON_PrintUnformatted(obj);
	cJSON_Delete(obj);
	return s ? s : "{}";
}

const char* nd500_dbg_mem_json(uint32_t addr, uint32_t len) {
	cJSON* root = cJSON_CreateObject();
	uint32_t cap = len;
	uint8_t* buf = (uint8_t*)malloc(cap);
	if (!buf) return "{}";
	size_t got = nd500_dbg_mem_dump(&g_machine, addr, len, buf, cap);
	cJSON_AddNumberToObject(root, "addr", addr);
	cJSON_AddNumberToObject(root, "len", (double)got);
	cJSON* arr = cJSON_CreateArray();
	cJSON* ascii_arr = cJSON_CreateArray();
	for (size_t i = 0; i < got; ++i) {
		char tmp[3];
		snprintf(tmp, sizeof(tmp), "%02X", buf[i]);
		cJSON_AddItemToArray(arr, cJSON_CreateString(tmp));
		
		// Add ASCII representation
		char ascii_char = (buf[i] >= 32 && buf[i] <= 126) ? buf[i] : '.';
		cJSON_AddItemToArray(ascii_arr, cJSON_CreateString((char[]){ascii_char, 0}));
	}
	cJSON_AddItemToObject(root, "bytes", arr);
	cJSON_AddItemToObject(root, "ascii", ascii_arr);
	free(buf);
	return dup_json_string(root);
}

const char* nd500_dbg_disasm_json(uint32_t addr, uint32_t len) {
    char json[16384];
    size_t n = nd500_disasm_format_range_json(&g_machine, addr, len, json, sizeof(json));
    if (n >= sizeof(json)) n = sizeof(json) - 1;
    json[n] = '\0';
    /* Optionally include symbol name */
    /* In WASM we did not load symbols; skip for now */
    return strdup(json);
}

const char* nd500_dbg_regs_json(void) {
	if (!g_machine.cpu) return "{}";
	Nd500Regs r; memset(&r, 0, sizeof(r));
	nd500_cpu_get_regs(g_machine.cpu, &r);
	cJSON* root = cJSON_CreateObject();
	cJSON_AddNumberToObject(root, "PC", r.PC);
	cJSON_AddNumberToObject(root, "FLAGS", r.FLAGS);
	cJSON* I = cJSON_CreateArray();
	for (int i = 0; i < 4; ++i) cJSON_AddItemToArray(I, cJSON_CreateNumber(r.I[i]));
	cJSON* A = cJSON_CreateArray();
	for (int i = 0; i < 4; ++i) cJSON_AddItemToArray(A, cJSON_CreateNumber(r.A[i]));
	cJSON* E = cJSON_CreateArray();
	for (int i = 0; i < 4; ++i) cJSON_AddItemToArray(E, cJSON_CreateNumber(r.E[i]));
	cJSON_AddItemToObject(root, "I", I);
	cJSON_AddItemToObject(root, "A", A);
	cJSON_AddItemToObject(root, "E", E);
	cJSON_AddNumberToObject(root, "L", r.L);
	cJSON_AddNumberToObject(root, "B", r.B);
	cJSON_AddNumberToObject(root, "R", r.R);
	cJSON_AddNumberToObject(root, "TOS", r.TOS);
	cJSON_AddNumberToObject(root, "LL", r.LL);
	cJSON_AddNumberToObject(root, "HL", r.HL);
	cJSON_AddNumberToObject(root, "THA", r.THA);
	cJSON_AddNumberToObject(root, "OTE1", r.OTE1);
	cJSON_AddNumberToObject(root, "OTE2", r.OTE2);
	cJSON_AddNumberToObject(root, "CTE1", r.CTE1);
	cJSON_AddNumberToObject(root, "CTE2", r.CTE2);
	cJSON_AddNumberToObject(root, "MTE1", r.MTE1);
	cJSON_AddNumberToObject(root, "MTE2", r.MTE2);
	cJSON_AddNumberToObject(root, "TEMM1", r.TEMM1);
	cJSON_AddNumberToObject(root, "TEMM2", r.TEMM2);
	/* MMU registers */
	cJSON_AddNumberToObject(root, "PSTP", r.PSTP);
	cJSON_AddNumberToObject(root, "DITBASE", r.DITBASE);
	cJSON_AddNumberToObject(root, "CED", r.CED);
	cJSON_AddNumberToObject(root, "CAD", r.CAD);
	cJSON_AddNumberToObject(root, "PS", r.PS);
	return dup_json_string(root);
}

void nd500_dbg_step_js(uint32_t n) { nd500_dbg_step(&g_machine, n ? n : 1); }
void nd500_dbg_run_js(void) { nd500_dbg_run(&g_machine); }
void nd500_dbg_stop_js(void) { nd500_dbg_stop(&g_machine); }
int nd500_dbg_load_aout_js(const uint8_t* data, uint32_t size) {
    uint32_t entry = 0;
    int rc = nd500_dbg_load_aout_buffer(&g_machine, data, size, &entry);
    if (rc == 0) {
        /* Set PC to entry (or 0 if not provided) */
        g_cpu.PC = entry;
    }
    return rc;
}

/* Load via path on MEMFS (browser) or node FS (ENVIRONMENT=node) */
int nd500_dbg_load_aout_path_js(const char* path) {
    if (!path) return -1;
    unsigned int entry = 0;
    int rc = ndlib_loadaout_file_ex(&g_machine, path, &entry, NULL);
    if (rc == 0) {
        /* Objects: entry often 0 or 4; we keep PC at 0 for objects per user policy */
        if (entry != 0 && entry != 4) g_cpu.PC = entry; else g_cpu.PC = 0;
        /* Load symbols for disassembly enhancement */
        ndlib_symbols_load(path);
    }
    return rc;
}

/* Breakpoint API functions */
int nd500_dbg_bp_add_js(uint32_t addr) {
	if (!g_machine.bp_mgr) return -1;
	return bp_add(g_machine.bp_mgr, addr, false);  // false = not one-shot
}

int nd500_dbg_bp_del_js(int id) {
	if (!g_machine.bp_mgr) return -1;
	return bp_delete(g_machine.bp_mgr, id);
}

int nd500_dbg_bp_enable_js(int id) {
	if (!g_machine.bp_mgr) return -1;
	return bp_enable(g_machine.bp_mgr, id);
}

int nd500_dbg_bp_disable_js(int id) {
	if (!g_machine.bp_mgr) return -1;
	return bp_disable(g_machine.bp_mgr, id);
}

const char* nd500_dbg_bp_list_json(void) {
	cJSON* root = cJSON_CreateArray();
	
	if (!g_machine.bp_mgr) {
		return dup_json_string(root);
	}
	
	// Get breakpoint list from manager
	Breakpoint* bps = g_machine.bp_mgr->breakpoints;
	for (int i = 0; i < g_machine.bp_mgr->bp_count; i++) {
		cJSON* bp_obj = cJSON_CreateObject();
		cJSON_AddNumberToObject(bp_obj, "id", i);
		cJSON_AddNumberToObject(bp_obj, "addr", bps[i].address);
		cJSON_AddBoolToObject(bp_obj, "enabled", bps[i].enabled);
		cJSON_AddItemToArray(root, bp_obj);
	}
	
	return dup_json_string(root);
}

const char* nd500_dbg_status_json(void) {
	cJSON* root = cJSON_CreateObject();
	cJSON_AddBoolToObject(root, "running", g_machine.run_flag);
	if (g_machine.cpu) {
		cJSON_AddNumberToObject(root, "pc", g_machine.cpu->PC);
	}
	cJSON_AddBoolToObject(root, "breakpoint_hit", 0); // TODO: implement breakpoint hit detection
	cJSON_AddStringToObject(root, "last_error", ""); // TODO: implement error tracking
	return dup_json_string(root);
}

const char* nd500_dbg_traps_json(void) {
	cJSON* root = cJSON_CreateObject();
	cJSON* traps = cJSON_CreateArray();
	
	if (nd500_dbg_trap_occurred()) {
		const char* desc = nd500_dbg_get_trap_description();
		cJSON* trap = cJSON_CreateObject();
		cJSON_AddStringToObject(trap, "description", desc ? desc : "Unknown trap");
		cJSON_AddBoolToObject(trap, "occurred", 1);
		cJSON_AddItemToArray(traps, trap);
	}
	
	cJSON_AddItemToObject(root, "traps", traps);
	return dup_json_string(root);
}

void nd500_dbg_clear_traps_js(void) {
	nd500_dbg_clear_traps();
}

void nd500_dbg_set_reg_js(const char* reg_name, uint32_t value) {
	if (!g_machine.cpu) return;

	// Map register names to CPU fields
	if (strcmp(reg_name, "PC") == 0) {
		g_machine.cpu->PC = value;
	} else if (strcmp(reg_name, "FLAGS") == 0) {
		g_machine.cpu->FLAGS = value;
	} else if (strncmp(reg_name, "I", 1) == 0 && strlen(reg_name) == 2) {
		int idx = reg_name[1] - '1';
		if (idx >= 0 && idx < 4) {
			g_machine.cpu->I[idx] = value;
		}
	} else if (strncmp(reg_name, "A", 1) == 0 && strlen(reg_name) == 2) {
		int idx = reg_name[1] - '1';
		if (idx >= 0 && idx < 4) {
			g_machine.cpu->A[idx] = value;
		}
	} else if (strncmp(reg_name, "E", 1) == 0 && strlen(reg_name) == 2) {
		int idx = reg_name[1] - '1';
		if (idx >= 0 && idx < 4) {
			g_machine.cpu->E[idx] = value;
		}
	} else if (strcmp(reg_name, "L") == 0) {
		g_machine.cpu->L = value;
	} else if (strcmp(reg_name, "B") == 0) {
		g_machine.cpu->B = value;
	} else if (strcmp(reg_name, "R") == 0) {
		g_machine.cpu->R = value;
	} else if (strcmp(reg_name, "TOS") == 0) {
		g_machine.cpu->TOS = value;
	} else if (strcmp(reg_name, "LL") == 0) {
		g_machine.cpu->LL = value;
	} else if (strcmp(reg_name, "HL") == 0) {
		g_machine.cpu->HL = value;
	} else if (strcmp(reg_name, "THA") == 0) {
		g_machine.cpu->THA = value;
	} else if (strcmp(reg_name, "PSTP") == 0) {
		g_machine.cpu->PSTP = value;
	} else if (strcmp(reg_name, "DITBASE") == 0) {
		g_machine.cpu->DITBASE = value;
	} else if (strcmp(reg_name, "CED") == 0) {
		g_machine.cpu->CED = value;
	} else if (strcmp(reg_name, "CAD") == 0) {
		g_machine.cpu->CAD = value;
	} else if (strcmp(reg_name, "PS") == 0) {
		g_machine.cpu->PS = value;
	}
}

/* Get all symbols as JSON array */
const char* nd500_dbg_symbols_json(void) {
	cJSON* root = cJSON_CreateArray();

	int count = ndlib_symbols_get_count();
	for (int i = 0; i < count; i++) {
		const char* name = ndlib_symbols_get_name(i);
		uint32_t addr = ndlib_symbols_get_addr(i);
		uint8_t type = ndlib_symbols_get_type(i);

		/* Skip unresolved/undefined symbols (type & 0x0E == 0x00) */
		if ((type & 0x0E) == 0x00) continue;

		cJSON* sym = cJSON_CreateObject();
		cJSON_AddStringToObject(sym, "name", name ? name : "");
		cJSON_AddNumberToObject(sym, "addr", addr);

		/* Add type description */
		const char* type_str = "UNKNOWN";
		if ((type & 0x0E) == 0x04) type_str = "TEXT";
		else if ((type & 0x0E) == 0x06) type_str = "DATA";
		else if ((type & 0x0E) == 0x08) type_str = "BSS";
		cJSON_AddStringToObject(sym, "type", type_str);

		cJSON_AddItemToArray(root, sym);
	}

	return dup_json_string(root);
}


/* ═══════════════════════════════════════════════════════ */
/* SHARED COMMAND LIBRARY WASM INTERFACE */
/* ═══════════════════════════════════════════════════════ */

/* Output buffer for WASM command execution */
static char g_wasm_output_buffer[16384];
static size_t g_wasm_output_pos = 0;

/* Output callback for WASM - appends to buffer */
static void wasm_output(const char* line, void* ctx) {
	(void)ctx; /* Unused */
	size_t len = strlen(line);
	if (g_wasm_output_pos + len + 1 < sizeof(g_wasm_output_buffer)) {
		memcpy(g_wasm_output_buffer + g_wasm_output_pos, line, len);
		g_wasm_output_pos += len;
		g_wasm_output_buffer[g_wasm_output_pos++] = '\n';
		g_wasm_output_buffer[g_wasm_output_pos] = '\0';
	}
}

/* Execute a debugger command and return output as string */
const char* nd500_cmd_exec_js(const char* cmdline) {
	if (!cmdline) return "";

	/* Clear output buffer */
	g_wasm_output_pos = 0;
	g_wasm_output_buffer[0] = '\0';

	/* Set up command context */
	CmdContext ctx = {
		.output = wasm_output,
		.error = wasm_output,  /* Errors also go to output buffer */
		.context = NULL
	};

	/* Execute command */
	int result = nd500_cmd_execute(&g_machine, cmdline, &ctx);

	/* Return output buffer (contains either success output or error messages) */
	/* Commands write error messages to output buffer via ctx.error callback */
	return strdup(g_wasm_output_buffer);
}

/* Get list of available commands as JSON array */
const char* nd500_cmd_list_js(void) {
	cJSON* root = cJSON_CreateArray();

	const char** commands = nd500_cmd_get_command_list();
	if (commands) {
		for (int i = 0; commands[i] != NULL; i++) {
			cJSON_AddItemToArray(root, cJSON_CreateString(commands[i]));
		}
	}

	return dup_json_string(root);
}

/* Get list of subcommands for a command as JSON array */
const char* nd500_cmd_subcommands_js(const char* command) {
	cJSON* root = cJSON_CreateArray();

	if (command) {
		const char** subcommands = nd500_cmd_get_subcommands(command);
		if (subcommands) {
			for (int i = 0; subcommands[i] != NULL; i++) {
				cJSON_AddItemToArray(root, cJSON_CreateString(subcommands[i]));
			}
		}
	}

	return dup_json_string(root);
}
