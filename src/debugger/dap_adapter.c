/*
 * dap_adapter.c - Debug Adapter Protocol adapter (port 4500)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
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
#include <strings.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include "debugger.h"
#include "../machine/machine_protos.h"
#include "../machine/breakpoints.h"
#include "../cpu/cpu_protos.h"
#include "../cpu/instruction_helpers.h"
#include "../cpu/float_exact.h"
#include "../cpu/nd500_mmu.h"
#include "../disasm/nd500_disasm.h"
#include "../ndlib/ndlib.h"
#include <ndmon/mon_file_table.h>
#include "../../external/libdap/libdap/include/dap_server.h"
#include "../../external/libdap/libdap/include/dap_server_cmds.h"

/* -- Adapter state ----------------------------------------------- */

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
/* The module's variables of a source map (ndlib_map_*). The Locals of a frame, and every
 * array and record that opens, have a reference from SCOPE_ID_HANDLE on (map_handle). */
#define SCOPE_ID_GLOBALS      1301

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

/* -- CPU lock ---------------------------------------------------- */

/* Every callback that reads or writes CPU registers, MMU state or machine
 * memory runs with the CPU lock held, because the thread advancing the CPU is
 * NOT always the one this file spawns through nd500_dbg_run: a domain started
 * from the SINTRAN shell's '@' prompt (--monitor) is stepped by run_domain in
 * src/frontend/nd500x/nd500x_shell.c, on the shell's own thread. Reading an
 * Nd500Cpu that another thread is in the middle of updating is a data race,
 * and a register or memory value obtained that way means nothing.
 *
 * DAP_CMD_LOCKED(cmd_x) { ... } defines the body as cmd_x_unlocked and a
 * cmd_x that takes the lock around it, so the registration table below is
 * unchanged and no body has to remember to unlock on each return path. */
#define DAP_CMD_LOCKED(name)                                    \
    static int name##_unlocked(DAPServer* server);          \
    static int name(DAPServer* server) {                    \
        int r;                                          \
        nd500_cpu_lock();                               \
        r = name##_unlocked(server);                    \
        nd500_cpu_unlock();                             \
        return r;                                       \
    }                                                       \
    static int name##_unlocked(DAPServer* server)

/* -- Helpers ----------------------------------------------------- */

static const char* stop_reason_to_dap(StopReason r) {
    switch (r) {
    case STOP_BREAKPOINT:        return "breakpoint";
    case STOP_STEP:              return "step";
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

/* Add one DAP execution breakpoint, plain or conditional, and fill the
 * DAP answer. A condition that does not evaluate, or a hit condition (not
 * supported), is answered as not verified and no breakpoint is set.
 * Returns 1 when a breakpoint was added. */
static int dap_add_exec_bp(uint32_t addr, const char* condition, const char* hit_condition,
                           DAPBreakpoint* out) {
    if (hit_condition && hit_condition[0] != '\0') {
        out->verified = false;
        out->message = strdup("Hit conditions are not supported");
        return 0;
    }
    int id;
    if (condition && condition[0] != '\0') {
        ExprResult check = bp_evaluate_condition(condition, g_machine->cpu);
        if (!check.valid) {
            char msg[160];
            snprintf(msg, sizeof(msg),
                     "Condition not understood: %s (form: REGISTER == != >= <= > < NUMBER)",
                     check.error_msg);
            out->verified = false;
            out->message = strdup(msg);
            return 0;
        }
        id = bp_add_conditional(g_machine->bp_mgr, g_machine->cpu, addr, condition, false);
    } else {
        id = bp_add(g_machine->bp_mgr, addr, false);
    }
    if (id < 0) {
        out->verified = false;
        out->message = strdup("Breakpoint table full");
        return 0;
    }
    out->verified = true;
    out->instruction_reference = addr;
    return 1;
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
    int rc = nd500_decode_at_peek(g_machine, addr, &fi);
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

/* -- Source map variables ---------------------------------------- */

/* The parts of a map name "NAME:TYPE" (ndlink -m, the type text of planc -g, src/fe/lower.c
 * dbg_type_put): the element size of a simple value (1 "byte", 2 "word", N "N bytes"; 0 for
 * any other text: a record, a descriptor, a pointer), and the bounds of a one-dimension
 * array "(LO:HI)" behind it. Returns the length of NAME. */
typedef struct {
    int name_len;
    const char* type;    /* behind the ':' ("" when there is none: one 32-bit word) */
    uint32_t elem;       /* bytes of one value; 0: not a simple value */
    int is_array;
    long lo, hi;
} MapType;

static void map_type_of(const char* name, MapType* t) {
    const char* colon = strchr(name, ':');
    memset(t, 0, sizeof(*t));
    t->name_len = colon ? (int)(colon - name) : (int)strlen(name);
    t->type = colon ? colon + 1 : "";
    const char* ty = t->type;
    if (*ty == '\0') {
        t->elem = 4;
    } else if (strncmp(ty, "byte", 4) == 0) {
        t->elem = 1; ty += 4;
    } else if (strncmp(ty, "word", 4) == 0) {
        t->elem = 2; ty += 4;
    } else if (*ty >= '0' && *ty <= '9') {
        char* end = NULL;
        unsigned long n = strtoul(ty, &end, 10);
        if (end && strncmp(end, " bytes", 6) == 0 && (n == 4)) {
            t->elem = (uint32_t)n;
            ty = end + 6;
        } else {
            return;
        }
    } else {
        return;
    }
    if (*ty == '(') {
        char* end = NULL;
        t->lo = strtol(ty + 1, &end, 10);
        if (end && *end == ':') {
            t->hi = strtol(end + 1, &end, 10);
            if (end && *end == ')' && end[1] == '\0' && t->hi >= t->lo) t->is_array = 1;
            else t->elem = 0;
        } else {
            t->elem = 0;
        }
    } else if (*ty != '\0') {
        t->elem = 0;
    }
}

/* Read n bytes (1, 2 or 4) at a data address through the data page tables; big-endian, as
 * the ND-500 keeps them. Returns 1 when all were read. */
static int map_read(uint32_t vaddr, uint32_t n, uint32_t* out) {
    uint8_t b[4] = {0, 0, 0, 0};
    if ((vaddr & 0x7FF) + n > 0x800) {
        for (uint32_t i = 0; i < n; i++) {
            uint32_t pa = dap_xlate_addr(vaddr + i, DAP_DATA_BP_ADDR_VIRTUAL);
            if (nd500_dbg_mem_read_raw(g_machine, pa, 1, b + i, 1) != 1) return 0;
        }
    } else {
        uint32_t pa = dap_xlate_addr(vaddr, DAP_DATA_BP_ADDR_VIRTUAL);
        if (nd500_dbg_mem_read_raw(g_machine, pa, n, b, n) != n) return 0;
    }
    uint32_t v = 0;
    for (uint32_t i = 0; i < n; i++) v = (v << 8) | b[i];
    *out = v;
    return 1;
}

/* The text of a simple value: decimal, then octal in brackets (the form nd100x shows). A
 * byte is 0..255, a word and a 4-byte value are signed. */
static void map_value_text(uint32_t v, uint32_t elem, char* buf, size_t cap) {
    long long d;
    if (elem == 1) d = (long long)(v & 0xFF);
    else if (elem == 2) d = (long long)(int16_t)(v & 0xFFFF);
    else d = (long long)(int32_t)v;
    snprintf(buf, cap, "%lld (%llo)", d, (unsigned long long)(elem == 1 ? (v & 0xFF) : elem == 2 ? (v & 0xFFFF) : v));
}

static DAPVariable* dap_add_variable(DAPServer* server, const char* name,
                                     const char* value, const char* type);

/* -- Values of the source map by their type text ------------------ */

/* The type text of planc -g (new-planc docs/debug-types.md, written by its src/fe/lower.c
 * dbg_type_put and carried unchanged by ndlink -m into the LOCAL and GLOBAL lines):
 *   TYPE := BASE [ "(" LO ":" HI ")" [ "+1" ] ]
 *   BASE := "byte" | "word" | N " bytes" | "real " N | "{" SIZE ";" COMP { "," COMP } "}"
 *           | "bits(" F ":" W ")"
 *   COMP := NAME "@" OFF ":" TYPE
 * and "desc " or "ref " in front of it for a parameter (map_add_typed); "ptr " is a local array
 * of the ND-100 and is not written for the ND-500. "real 4" and "real 8" are a REAL and a REAL8
 * (ND-05.009.4 7.2.5), shown by nd500_fx_text. An empty text is one 32-bit value (a name with
 * no type). Sizes and offsets are in bytes. An array's element k lies (k - LO) * (bytes of the
 * element) behind the address, one byte later with "+1"; a component OFF bytes behind the start
 * of its record. */
enum { MAP_TY_SIMPLE = 1, MAP_TY_RECORD = 2, MAP_TY_OTHER = 3, MAP_TY_REAL = 4 };

typedef struct {
    int kind;             /* MAP_TY_SIMPLE, MAP_TY_RECORD, MAP_TY_REAL, MAP_TY_OTHER (no reader) */
    uint32_t size;        /* bytes of BASE: 1, 2, N, 4 or 8 for a REAL, or the SIZE of a record */
    const char* comps;    /* a record: its first COMP */
    const char* base_end; /* first character behind BASE */
    const char* end;      /* first character behind the whole TYPE */
    int is_array;
    long lo, hi;
    int plus1;
} MapTy;

/* The first character behind the TYPE that starts at t: its end, a ',' or the '}' of the
 * record it is a component of. */
static const char* map_ty_end(const char* t) {
    int depth = 0;
    for (; *t; t++) {
        if (*t == '{') depth++;
        else if (*t == '}') { if (depth == 0) break; depth--; }
        else if (*t == ',' && depth == 0) break;
    }
    return t;
}

static void map_ty_parse(const char* t, MapTy* m) {
    char* e = NULL;
    memset(m, 0, sizeof(*m));
    m->end = map_ty_end(t);
    m->kind = MAP_TY_OTHER;
    m->base_end = m->end;
    if (t == m->end) {
        m->kind = MAP_TY_SIMPLE;
        m->size = 4;
        return;
    }
    const char* p = t;
    if (*p == '{') {
        unsigned long size = strtoul(p + 1, &e, 10);
        if (!e || *e != ';') return;
        m->comps = e + 1;
        int depth = 0;
        for (p++; *p; p++) {
            if (*p == '{') depth++;
            else if (*p == '}') { if (depth == 0) break; depth--; }
        }
        if (*p != '}') return;
        p++;
        m->kind = MAP_TY_RECORD;
        m->size = (uint32_t)size;
    } else if (strncmp(p, "byte", 4) == 0) {
        m->kind = MAP_TY_SIMPLE; m->size = 1; p += 4;
    } else if (strncmp(p, "word", 4) == 0) {
        m->kind = MAP_TY_SIMPLE; m->size = 2; p += 4;
    } else if (*p >= '0' && *p <= '9') {
        unsigned long n = strtoul(p, &e, 10);
        if (!e || strncmp(e, " bytes", 6) != 0 || n == 0) return;
        m->kind = MAP_TY_SIMPLE; m->size = (uint32_t)n; p = e + 6;
    } else if (strncmp(p, "real ", 5) == 0) {
        unsigned long n = strtoul(p + 5, &e, 10);
        if (!e || (n != 4 && n != 8)) return;
        m->kind = MAP_TY_REAL; m->size = (uint32_t)n; p = e;
    } else {
        return; /* bits(F:W), desc, ptr, ref: not read here */
    }
    m->base_end = p;
    if (p == m->end) return;
    if (*p != '(') { m->kind = MAP_TY_OTHER; return; }
    m->lo = strtol(p + 1, &e, 10);
    if (!e || *e != ':') { m->kind = MAP_TY_OTHER; return; }
    m->hi = strtol(e + 1, &e, 10);
    if (!e || *e != ')' || m->hi < m->lo) { m->kind = MAP_TY_OTHER; return; }
    p = e + 1;
    if (strncmp(p, "+1", 2) == 0) { m->plus1 = 1; p += 2; }
    if (p != m->end) { m->kind = MAP_TY_OTHER; return; }
    m->is_array = 1;
}

/* A variables reference that opens a value of the source map: the type text (inside the map's
 * own strings, kept until the next map is loaded) and the address, or the Locals of routine
 * `func` of the map at frame base `addr`. The value is read from memory again each time it is
 * opened. The same (text, address) always gives the same reference. */
typedef struct {
    const char* type;
    uint32_t addr;
    int elem;  /* 1: the element of the array `type` (its BASE only) */
    int func;  /* >= 0: the Locals of that routine of the map, at B = addr */
    int desc;  /* 1: an array parameter: `type` is its element, addr the address of element lo,
                  the bounds lo:hi read from its descriptor (map_add_typed) */
    long lo, hi;
} MapHandle;

#define SCOPE_ID_HANDLE 100000
#define MAP_HANDLE_MAX  60000

static MapHandle* g_map_handles = NULL;
static int g_map_handle_count = 0;
static int g_map_handle_cap = 0;

static void map_handles_clear(void) {
    g_map_handle_count = 0;
}

/* The reference of a handle, 0 when the table is full (the value then does not open). */
static int map_handle_full(const char* type, uint32_t addr, int elem, int func, int desc,
                           long lo, long hi) {
    for (int i = 0; i < g_map_handle_count; i++) {
        const MapHandle* h = &g_map_handles[i];
        if (h->type == type && h->addr == addr && h->elem == elem && h->func == func &&
            h->desc == desc && h->lo == lo && h->hi == hi)
            return SCOPE_ID_HANDLE + i;
    }
    if (g_map_handle_count >= MAP_HANDLE_MAX) return 0;
    if (g_map_handle_count == g_map_handle_cap) {
        int cap = g_map_handle_cap ? g_map_handle_cap * 2 : 256;
        MapHandle* bigger = (MapHandle*)realloc(g_map_handles, (size_t)cap * sizeof(MapHandle));
        if (!bigger) return 0;
        g_map_handles = bigger;
        g_map_handle_cap = cap;
    }
    MapHandle* h = &g_map_handles[g_map_handle_count];
    h->type = type;
    h->addr = addr;
    h->elem = elem;
    h->func = func;
    h->desc = desc;
    h->lo = lo;
    h->hi = hi;
    return SCOPE_ID_HANDLE + g_map_handle_count++;
}

static int map_handle(const char* type, uint32_t addr, int elem, int func) {
    return map_handle_full(type, addr, elem, func, 0, 0, 0);
}

/* Read n bytes at a data address and give them as hex text ("0x0011223344556677"). */
static void map_hex_text(uint32_t addr, uint32_t n, char* buf, size_t cap) {
    size_t o = (size_t)snprintf(buf, cap, "0x");
    for (uint32_t i = 0; i < n && o + 3 < cap; i++) {
        uint32_t b = 0;
        if (!map_read(addr + i, 1, &b)) { snprintf(buf, cap, "(not readable at 0x%08X)", addr + i); return; }
        o += (size_t)snprintf(buf + o, cap - o, "%02X", b & 0xFF);
    }
}

/* The number of components of the record whose first COMP is at p. */
static int map_comp_count(const char* p) {
    int n = 0;
    while (*p && *p != '}') {
        const char* colon = strchr(p, ':');
        if (!colon) break;
        n++;
        p = map_ty_end(colon + 1);
        if (*p == ',') p++;
    }
    return n;
}

/* The value of a REAL of size bytes (4 or 8) at addr: the bits read as they lie (big-endian),
 * the exact decimal of their value (nd500_fx_text: no host float on the way; more than 40
 * digits are cut, "..." marks that). Used by the variables view and by logpoints, so both
 * show the same text. */
static void map_real_text(uint32_t addr, uint32_t size, char* out, size_t cap) {
    uint32_t hi = 0, lo = 0;
    if (map_read(addr, 4, &hi) && (size == 4 || map_read(addr + 4, 4, &lo))) {
        uint64_t bits = size == 8 ? (((uint64_t)hi << 32) | lo) : hi;
        char num[72];
        nd500_fx_text(bits, size == 8, num, sizeof num);
        snprintf(out, cap, "%s", num);
    } else {
        snprintf(out, cap, "(not readable at 0x%08X)", addr);
    }
}

/* One variable NAME of type text t at address addr. elem: show only the element of the
 * array t (its BASE). */
static void map_add_typed(DAPServer* server, const char* name, const char* t, uint32_t addr, int elem) {
    MapTy m;
    char val[96], ty[256];
    if (strncmp(t, "ref ", 4) == 0 || strncmp(t, "desc ", 5) == 0) {
        /* A parameter (new-planc docs/debug-types.md). PLANC-500 G passes a record as one
         * 32-bit word that holds its BYTE address ("ref "), an array or BYTES as a descriptor
         * of three 32-bit words in the frame: the BYTE address index 0 would have, the lowest
         * and the highest index ("desc ", the element's text behind it). Measured on the real
         * compiler's listings: new-planc docs/be-nd500/code-patterns.md 1.5 and 3.4. */
        int is_desc = t[0] == 'd';
        const char* inner = t + (is_desc ? 5 : 4);
        uint32_t a0 = 0, lo = 0, hi = 0;
        if (!map_read(addr, 4, &a0) ||
            (is_desc && (!map_read(addr + 4, 4, &lo) || !map_read(addr + 8, 4, &hi)))) {
            snprintf(val, sizeof val, "(not readable at 0x%08X)", addr);
            DAPVariable* v = dap_add_variable(server, name, val, t);
            if (v) v->memory_reference = (int)addr;
            return;
        }
        if (!is_desc) {
            map_add_typed(server, name, inner, a0, 0);
            return;
        }
        map_ty_parse(inner, &m);
        long l = (long)(int32_t)lo, h = (long)(int32_t)hi;
        uint32_t first = a0 + (uint32_t)(l * (long)m.size);
        snprintf(ty, sizeof ty, "%s", t);
        if (m.kind == MAP_TY_OTHER || m.is_array || m.size == 0 || h < l) {
            snprintf(val, sizeof val, "descriptor 0x%08X (%ld:%ld)", a0, l, h);
            DAPVariable* v = dap_add_variable(server, name, val, ty);
            if (v) v->memory_reference = (int)addr;
            return;
        }
        snprintf(val, sizeof val, "array (%ld:%ld) @0x%08X", l, h, first);
        DAPVariable* v = dap_add_variable(server, name, val, ty);
        if (v) {
            v->variables_reference = map_handle_full(inner, first, 0, -1, 1, l, h);
            v->indexed_variables = (int)(h - l + 1);
            v->memory_reference = (int)first;
        }
        return;
    }
    map_ty_parse(t, &m);
    if (elem) m.is_array = 0;
    const char* tend = elem ? m.base_end : m.end;
    snprintf(ty, sizeof ty, "%.*s", (int)(tend - t), t);
    if (m.kind == MAP_TY_OTHER) {
        snprintf(val, sizeof val, "@0x%08X (no reader for this type text)", addr);
        DAPVariable* v = dap_add_variable(server, name, val, ty);
        if (v) v->memory_reference = (int)addr;
        return;
    }
    if (m.is_array) {
        snprintf(val, sizeof val, "array (%ld:%ld) @0x%08X", m.lo, m.hi, addr);
        DAPVariable* v = dap_add_variable(server, name, val, ty);
        if (v) {
            v->variables_reference = map_handle(t, addr, 0, -1);
            v->indexed_variables = (int)(m.hi - m.lo + 1);
            v->memory_reference = (int)addr;
        }
        return;
    }
    if (m.kind == MAP_TY_RECORD) {
        snprintf(val, sizeof val, "record (%u bytes) @0x%08X", (unsigned)m.size, addr);
        DAPVariable* v = dap_add_variable(server, name, val, ty);
        if (v) {
            v->variables_reference = map_handle(t, addr, elem, -1);
            v->named_variables = map_comp_count(m.comps);
            v->memory_reference = (int)addr;
        }
        return;
    }
    uint32_t raw = 0;
    if (m.kind == MAP_TY_REAL) {
        /* the machine's own float: the bits read as they lie (big-endian), the exact decimal
         * of their value (nd500_fx_text: no host float on the way; more than 40 digits are cut,
         * "..." marks that) */
        map_real_text(addr, m.size, val, sizeof val);
    } else if (m.size == 1 || m.size == 2 || m.size == 4) {
        if (map_read(addr, m.size, &raw)) map_value_text(raw, m.size, val, sizeof val);
        else snprintf(val, sizeof val, "(not readable at 0x%08X)", addr);
    } else {
        /* a value of another size (REAL8, a set): its bytes as they lie, high byte first */
        map_hex_text(addr, m.size > 16 ? 16 : m.size, val, sizeof val);
    }
    DAPVariable* v = dap_add_variable(server, name, val, ty);
    if (v) v->memory_reference = (int)addr;
}

/* The children of the value of a handle: the elements of an array, the components of a
 * record. */
static void map_add_children(DAPServer* server, const MapHandle* h) {
    MapTy m;
    char nm[96];
    map_ty_parse(h->type, &m);
    if (h->desc) {
        /* the elements of an array parameter: h->type is the element, h->addr element lo */
        for (long i = h->lo; i <= h->hi && i - h->lo < 4096; i++) {
            snprintf(nm, sizeof nm, "[%ld]", i);
            map_add_typed(server, nm, h->type, h->addr + (uint32_t)(i - h->lo) * m.size, 0);
        }
        return;
    }
    if (m.is_array && !h->elem) {
        uint32_t esize = m.size;
        uint32_t first = h->addr + (m.plus1 ? 1u : 0u);
        if (esize == 0) return;
        for (long i = m.lo; i <= m.hi && i - m.lo < 4096; i++) {
            snprintf(nm, sizeof nm, "[%ld]", i);
            map_add_typed(server, nm, h->type, first + (uint32_t)(i - m.lo) * esize, 1);
        }
        return;
    }
    if (m.kind != MAP_TY_RECORD) return;
    const char* p = m.comps;
    while (*p && *p != '}') {
        const char* at = strchr(p, '@');
        if (!at) break;
        char* e = NULL;
        unsigned long off = strtoul(at + 1, &e, 10);
        if (!e || *e != ':') break;
        snprintf(nm, sizeof nm, "%.*s", (int)(at - p), p);
        map_add_typed(server, nm, e + 1, h->addr + (uint32_t)off, 0);
        p = map_ty_end(e + 1);
        if (*p == ',') p++;
    }
}

/* One variable of the source map, "NAME:TYPE" (or "NAME"), at address addr. */
static void map_add_var(DAPServer* server, const char* name, uint32_t addr) {
    const char* colon = strchr(name, ':');
    char nm[96];
    snprintf(nm, sizeof nm, "%.*s", colon ? (int)(colon - name) : (int)strlen(name), name);
    map_add_typed(server, nm, colon ? colon + 1 : name + strlen(name), addr, 0);
}

static uint32_t map_frame_base(void) {
    uint32_t b = 0;
    nd500_dbg_reg_get_by_name(g_machine->cpu, "B", &b);
    return b;
}

/* -- The frames of the call chain -------------------------------- */

/* One frame: the address it runs at, the address its routine and source line are looked up
 * by, and its frame base B. */
typedef struct {
    uint32_t pc;
    uint32_t look;
    uint32_t b;
} MapFrame;

#define MAP_FRAMES_MAX 64

/* The frames from the PC's down to the first one, at most max. A routine entered by CALL and
 * ENTS (and the other ENT instructions, src/cpu/instructions/CALL/Ents.c) keeps the B of its
 * caller at B+0 (PREVB) and the address after the CALL at B+4 (RETA); B goes up on a call
 * (measured on new-planc BE01: MAIN B=0x08000004, LOC B=0x0800001C). The walk ends at a PREVB
 * or RETA of 0, at a PREVB equal to B, or where memory cannot be read. A caller's line is that
 * of RETA - 1: the last byte of its CALL.
 * At the first instruction of a routine of the map (its ENT, not yet run) no frame is made
 * yet: the caller is the frame B still is, at the return address the CALL left for the ENT.
 * A routine inside a routine has no frame of its own (PLANC-500 G, new-planc
 * docs/be-nd500/code-patterns.md 2.4, probe GEN07): it starts with ENTD (9C: L := the return
 * address, src/cpu/instructions/CALL/Entd.c) and then `L =: B.x`, which keeps L in a slot of
 * the frame of the routine around it; it returns by JUMPG B.x. Its caller is therefore the
 * address in that slot, in the same frame B (map_nested_return); only when the walk reaches a
 * routine that is not of this kind is the next caller found by PREVB and RETA. */

/* The return address of the routine inside a routine that holds pc, when pc is in one (its
 * entry byte at the FUNC address of the map is ENTD, 9C), in frame b. At the ENTD itself
 * (not yet run) the address is the one the CALL left for it; on the `L =: B.x` after it, L;
 * further on, the slot B.x that instruction names, read in frame b. Returns 1 with *ret set,
 * 0 when pc is not in such a routine, -1 when it is one but the slot cannot be read (the
 * instruction after the ENTD is not `L =:` to a B-relative place, or memory cannot be read). */
static int map_nested_return(uint32_t pc, uint32_t b, int top, uint32_t* ret) {
    Nd500Cpu* cpu = g_machine->cpu;
    uint32_t entry = 0;
    int mf = ndlib_map_func_for_pc(pc);
    if (mf < 0 || !ndlib_map_func(mf, &entry, NULL)) return 0;
    Nd500FetchedInstruction fi;
    memset(&fi, 0, sizeof fi);
    /* the code is read the way the disassembler reads it (program space) */
    if (nd500_decode_at_peek(g_machine, entry, &fi) != 0 || fi.opcode != 0x9C) return 0;
    if (pc == entry) {
        if (!top || cpu->pending_call_return_address == 0) return -1;
        *ret = cpu->pending_call_return_address;
        return 1;
    }
    memset(&fi, 0, sizeof fi);
    if (nd500_decode_at_peek(g_machine, entry + 1, &fi) != 0 || fi.opcode != 0xFDC0 ||
        fi.operand_count < 1 ||
        (fi.operands[0].mode != ND500_ADDR_LOCAL_SHORT && fi.operands[0].mode != ND500_ADDR_LOCAL))
        return -1;
    if (pc == entry + 1) {
        if (!top) return -1;
        *ret = cpu->L;
        return 1;
    }
    /* the decoder works the address out with the B of the CPU; the slot is that place
     * relative to B, read in frame b */
    uint32_t slot = fi.operands[0].effective_address - cpu->B;
    return map_read(b + slot, 4, ret) ? 1 : -1;
}

static int map_frames(MapFrame* fr, int max) {
    Nd500Cpu* cpu = g_machine->cpu;
    uint32_t b = map_frame_base();
    int n = 0;
    fr[n].pc = cpu->PC;
    fr[n].look = cpu->PC;
    fr[n].b = b;
    n++;
    uint32_t entry = 0;
    uint32_t nret = 0;
    int mf = ndlib_map_func_for_pc(cpu->PC);
    if (map_nested_return(cpu->PC, b, 1, &nret) == 0 && mf >= 0 &&
        ndlib_map_func(mf, &entry, NULL) && entry == cpu->PC &&
        cpu->pending_call_return_address != 0 && n < max) {
        fr[n].pc = cpu->pending_call_return_address;
        fr[n].look = fr[n].pc - 1;
        fr[n].b = b;
        n++;
    }
    while (n < max) {
        /* a routine inside a routine: its caller in the same frame (map_nested_return) */
        int nested = map_nested_return(fr[n - 1].pc, b, n == 1, &nret);
        if (nested < 0) break;
        if (nested > 0) {
            fr[n].pc = nret;
            fr[n].look = nret - 1;
            fr[n].b = b;
            n++;
            continue;
        }
        uint32_t prevb = 0, reta = 0;
        if (!map_read(b + 0, 4, &prevb) || !map_read(b + 4, 4, &reta)) break;
        if (prevb == 0 || reta == 0 || prevb == b) break;
        fr[n].pc = reta;
        fr[n].look = reta - 1;
        fr[n].b = prevb;
        n++;
        b = prevb;
    }
    return n;
}

/* -- Logpoints --------------------------------------------------- */

/* The texts of logpoints hit on the CPU thread, sent by cmd_check_cpu_events as output
 * events (a send from the CPU thread would race the DAP thread on the socket). */
static pthread_mutex_t g_log_lock = PTHREAD_MUTEX_INITIALIZER;
static char g_log_buf[8192];
static size_t g_log_len = 0;

/* A REAL (type text "real 4" / "real 8", not an array) for a logpoint: the same text the
 * variables view shows (map_real_text). 0 for any other type text. */
static int log_real_text(const char* type, uint32_t addr, char* out, size_t cap) {
    MapTy m;
    map_ty_parse(type, &m);
    if (m.kind != MAP_TY_REAL || m.is_array) return 0;
    map_real_text(addr, m.size, out, cap);
    return 1;
}

/* The value of {NAME} in a log message: a variable of the source map (a Global, or a Local
 * of the routine of the PC), else a register; 0 when there is none. */
static int log_lookup(const char* name, size_t n, char* out, size_t cap) {
    uint32_t pc = g_machine->cpu->PC;
    int f = ndlib_map_func_for_pc(pc);
    int locals = 0;
    if (f >= 0) (void)ndlib_map_func(f, NULL, &locals);
    for (int i = 0; i < locals; i++) {
        int32_t off = 0;
        const char* ln = ndlib_map_local(f, i, &off);
        MapType t;
        map_type_of(ln, &t);
        uint32_t raw;
        if ((size_t)t.name_len == n && strncasecmp(ln, name, n) == 0 && t.elem && !t.is_array &&
            map_read(map_frame_base() + (uint32_t)off, t.elem, &raw)) {
            map_value_text(raw, t.elem, out, cap);
            return 1;
        }
        if ((size_t)t.name_len == n && strncasecmp(ln, name, n) == 0 &&
            log_real_text(t.type, map_frame_base() + (uint32_t)off, out, cap))
            return 1;
    }
    for (int i = 0; i < ndlib_map_global_count(); i++) {
        uint32_t addr = 0, raw;
        const char* gn = ndlib_map_global(i, &addr);
        MapType t;
        map_type_of(gn, &t);
        if ((size_t)t.name_len == n && strncasecmp(gn, name, n) == 0 && t.elem && !t.is_array &&
            map_read(addr, t.elem, &raw)) {
            map_value_text(raw, t.elem, out, cap);
            return 1;
        }
        if ((size_t)t.name_len == n && strncasecmp(gn, name, n) == 0 && log_real_text(t.type, addr, out, cap))
            return 1;
    }
    char reg[16];
    uint32_t v;
    if (n < sizeof reg) {
        memcpy(reg, name, n);
        reg[n] = '\0';
        if (nd500_dbg_reg_get_by_name(g_machine->cpu, reg, &v) == 0) {
            snprintf(out, cap, "0x%08X", v);
            return 1;
        }
    }
    return 0;
}

/* BreakpointManager.log_hook: the text with each {NAME} replaced by its value (an unknown
 * name stays as written), one line, queued for the client. Runs on the CPU thread. */
static void dap_log_hook(const char* text, struct Nd500Cpu* cpu) {
    (void)cpu;
    char line[512];
    size_t o = 0;
    for (const char* p = text; *p && o + 1 < sizeof line; p++) {
        const char* close = (*p == '{') ? strchr(p, '}') : NULL;
        char val[96];
        if (close && log_lookup(p + 1, (size_t)(close - p - 1), val, sizeof val)) {
            size_t k = strlen(val);
            if (o + k >= sizeof line - 1) k = sizeof line - 2 - o;
            memcpy(line + o, val, k);
            o += k;
            p = close;
        } else {
            line[o++] = *p;
        }
    }
    line[o++] = '\n';
    line[o] = '\0';
    pthread_mutex_lock(&g_log_lock);
    if (g_log_len + o < sizeof g_log_buf) {
        memcpy(g_log_buf + g_log_len, line, o + 1);
        g_log_len += o;
    }
    pthread_mutex_unlock(&g_log_lock);
}

static void dap_flush_log(DAPServer* server) {
    char out[sizeof g_log_buf];
    pthread_mutex_lock(&g_log_lock);
    size_t n = g_log_len;
    if (n) memcpy(out, g_log_buf, n + 1);
    g_log_len = 0;
    pthread_mutex_unlock(&g_log_lock);
    if (n) dap_server_send_output_category(server, DAP_OUTPUT_CONSOLE, out);
}

/* The end of the program, told by the loop that ran it (nd500_dap_program_ended) and sent by
 * cmd_check_cpu_events. The loop goes on at once (the shell reads its next command, often
 * EXIT), so the end cannot be read off the machine's stop reason when the DAP thread looks. */
static volatile int g_exit_pending = 0;
static volatile int g_exit_code = 0;

void nd500_dap_program_ended(int exit_code) {
    if (!g_server) return;
    g_exit_code = exit_code;
    g_exit_pending = 1;
}

/* -- Source line steps ------------------------------------------- */

/* A step over (next), into (stepIn) or out of (stepOut) a source line, run by the loop that
 * drives the CPU: line_step_hook is called before each instruction (Nd500Machine.step_hook)
 * and ends the step at the first address of a source line (the line entries of the map):
 *   in   any line start other than the one the step began at;
 *   over a line start in the same frame (B as at the start) or in a frame that called it;
 *   out  a line start in a frame that called it.
 * "The same frame or one that called it": the call chain (map_frames, the frames the stack
 * trace shows) is as deep as at the start or less. B alone cannot say it: a routine inside a
 * routine runs in the frame of the routine around it (same B, map_nested_return), and a
 * routine with a stack of its own (INISTACK in a routine, ENTM) has its B at the bottom of
 * that stack, wherever the array lies in the data (below its caller's B when the array is
 * declared before the main program's stack). The chain counts both. */
enum { STEP_IN, STEP_OVER, STEP_OUT };
static struct {
    int kind;
    uint32_t start_pc;
    int start_depth;     /* frames of the call chain at the start (map_frames) */
    int moved;           /* 0 until the first instruction of the step has run */
} g_ls;

static int line_step_hook(Nd500Machine* m, uint32_t pc) {
    if (!g_ls.moved) {
        g_ls.moved = 1;
        return 0;
    }
    if (!ndlib_map_is_line_start(pc, NULL, NULL)) return 0;
    (void)m;
    if (g_ls.kind == STEP_IN) return 1;
    MapFrame chain[MAP_FRAMES_MAX];
    int depth = map_frames(chain, MAP_FRAMES_MAX);
    return g_ls.kind == STEP_OVER ? depth <= g_ls.start_depth : depth < g_ls.start_depth;
}

/* -- Lifecycle callbacks ----------------------------------------- */

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
DAP_CMD_LOCKED(cmd_check_cpu_events) {
    if (!g_machine) return 0;

    dap_flush_console_output(server);
    dap_flush_log(server);

    if (g_exit_pending) {
        /* The program ended (nd500_dap_program_ended): for the client that is the end of the
         * debuggee, "exited" then "terminated", not a stop it could continue. */
        int code = g_exit_code;
        g_exit_pending = 0;
        g_machine->step_hook = NULL;
        g_step_pending = 0;
        dap_flush_log(server);
        dap_server_send_exited_event(server, code);
        dap_server_send_terminated_event(server, false);
        return 0;
    }
    /* STOP_MON_HALT is the end of the program, sent as exited/terminated (g_exit_pending) */
    if (g_machine->stop_reason != STOP_NONE && g_machine->stop_reason != STOP_MON_HALT &&
        !g_machine->run_flag) {
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

/* -- Session callbacks ------------------------------------------- */

static void dap_load_map(DAPServer* server);

DAP_CMD_LOCKED(cmd_launch) {
    if (!g_machine) return -1;
    if (g_machine->bp_mgr) g_machine->bp_mgr->log_hook = dap_log_hook;
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

    dap_load_map(server);   /* after the program's own symbols: a map given wins */
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

/* The source map the client names (attach/launch "mapFile", ndlink -m): line entries for
 * breakpoints, steps and the stack trace, and the names of the Locals and Globals scopes. */
static void dap_load_map(DAPServer* server) {
    const char* map = server->debugger_state.map_path;
    char msg[600];
    if (!map || !*map) return;
    map_handles_clear(); /* the handles point into the strings of the map loaded before */
    if (ndlib_map_load(map) != 0) {
        snprintf(msg, sizeof msg, "Cannot read the source map %s\n", map);
        dap_server_send_output_category(server, DAP_OUTPUT_STDERR, msg);
        return;
    }
    snprintf(msg, sizeof msg, "Source map %s: %d line entries, %d variables of the module\n", map,
             ndlib_map_line_count(), ndlib_map_global_count());
    dap_server_send_output_category(server, DAP_OUTPUT_CONSOLE, msg);
}

DAP_CMD_LOCKED(cmd_attach) {
    if (!g_machine) return -1;
    dap_load_map(server);
    if (g_machine->bp_mgr) g_machine->bp_mgr->log_hook = dap_log_hook;
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

DAP_CMD_LOCKED(cmd_restart) {
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

/* -- Execution control ------------------------------------------- */

DAP_CMD_LOCKED(cmd_continue) {
    if (!g_machine) return -1;
    g_machine->stop_reason = STOP_NONE;
    g_machine->step_hook = NULL;   /* a continue ends a source line step that did not end */
    g_step_pending = 0;
    server->debugger_state.has_stopped = false;
    server->current_command.context.continue_cmd.all_threads_continue = true;
    nd500_dbg_run(g_machine);
    return 0;
}

/* All step flavors are single-instruction steps: the ND-500 adapter has
 * no reliable source-line stepping yet, and step-out would need frame
 * unwinding. Executed synchronously (one instruction is fast). */
DAP_CMD_LOCKED(cmd_step_common) {
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

/* A source line step when the map has line entries and the PC is in a line of it (see
 * line_step_hook); else the single instruction step above. */
static int dap_line_step(DAPServer* server, int kind) {
    if (!g_machine || !g_machine->cpu) return -1;
    if (g_machine->run_flag) return -1;
    if (ndlib_map_line_count() == 0 ||
        ndlib_symbols_source_for_pc(g_machine->cpu->PC, NULL, NULL) != 1) {
        return cmd_step_common(server);
    }
    nd500_cpu_lock();
    g_ls.kind = kind;
    g_ls.start_pc = g_machine->cpu->PC;
    {
        MapFrame chain[MAP_FRAMES_MAX];
        g_ls.start_depth = map_frames(chain, MAP_FRAMES_MAX);
    }
    g_ls.moved = 0;
    g_machine->stop_reason = STOP_NONE;
    g_machine->step_hook = line_step_hook;
    server->debugger_state.has_stopped = false;
    nd500_cpu_unlock();
    nd500_dbg_run(g_machine);
    return 0;
}

static int cmd_next(DAPServer* server)     { return dap_line_step(server, STEP_OVER); }
static int cmd_step_in(DAPServer* server)  { return dap_line_step(server, STEP_IN); }
static int cmd_step_out(DAPServer* server) { return dap_line_step(server, STEP_OUT); }

DAP_CMD_LOCKED(cmd_pause) {
    (void)server;
    if (!g_machine) return -1;
    nd500_dbg_stop(g_machine);
    /* libdap's pause handler sends the 'stopped' event itself;
     * do not set a stop reason here or the client gets a duplicate. */
    return 0;
}

/* -- Breakpoints ------------------------------------------------- */

DAP_CMD_LOCKED(cmd_set_breakpoints) {
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
                const char* bs = strrchr(ctx->source_path, '\\');
                if (bs && (!base || bs > base)) base = bs;   /* a Windows path */
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
        if (!dap_add_exec_bp(addr, bp->condition, bp->hit_condition, bp)) {
            continue;
        }
        /* a logpoint: the breakpoint just added says its text instead of stopping */
        if (bp->log_message && bp->log_message[0] != '\0' &&
            bp_set_log_message(g_machine->bp_mgr, g_machine->bp_mgr->bp_count - 1,
                               bp->log_message) != 0) {
            bp_delete(g_machine->bp_mgr, g_machine->bp_mgr->bp_count - 1);
            bp->verified = false;
            bp->message = strdup("Log message longer than 159 characters");
            continue;
        }
        if (g_dap_src_bp_count < DAP_MAX_TRACKED) {
            g_dap_src_bp_addrs[g_dap_src_bp_count++] = addr;
        }
    }
    return 0;
}

DAP_CMD_LOCKED(cmd_set_instruction_breakpoints) {
    if (!g_machine || !g_machine->bp_mgr) return -1;
    InstructionBreakpointCommandContext* ctx =
        &server->current_command.context.instruction_breakpoint;

    dap_remove_tracked_bps(g_machine->bp_mgr, g_dap_instr_bp_addrs, g_dap_instr_bp_count);
    g_dap_instr_bp_count = 0;

    for (int i = 0; i < ctx->breakpoint_count; i++) {
        uint32_t addr = ctx->addresses[i] + (uint32_t)(ctx->offsets ? ctx->offsets[i] : 0);
        DAPBreakpoint* bp = &ctx->breakpoints[i];
        const char* cond = ctx->conditions ? ctx->conditions[i] : NULL;
        if (!dap_add_exec_bp(addr, cond, bp->hit_condition, bp)) {
            continue;
        }
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
DAP_CMD_LOCKED(cmd_data_breakpoint_info) {
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

DAP_CMD_LOCKED(cmd_set_data_breakpoints) {
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

/* -- Inspection -------------------------------------------------- */

DAP_CMD_LOCKED(cmd_stack_trace) {
    StackTraceCommandContext* ctx = &server->current_command.context.stack_trace;
    ctx->frames = NULL;
    ctx->frame_count = 0;
    ctx->total_frames = 0;
    if (!g_machine || !g_machine->cpu) return -1;

    /* The frames of the call chain (map_frames: PREVB and RETA of each frame) */
    MapFrame chain[MAP_FRAMES_MAX];
    int count = map_frames(chain, MAP_FRAMES_MAX);
    ctx->frames = (DAPStackFrame*)calloc((size_t)count, sizeof(DAPStackFrame));
    if (!ctx->frames) return -1;

    for (int k = 0; k < count; k++) {
        uint32_t pc = chain[k].pc;
        DAPStackFrame* f = &ctx->frames[k];
        f->id = k;
        /* the name: a symbol at exactly this address (offset 0), else the routine of the
         * source map that holds it, the offset counted from its entry address (libdap
         * writes "NAME +OFFSET @PC" from symbol_entry_point) */
        const char* sym = ndlib_symbols_name_for_addr(chain[k].look);
        uint32_t entry = chain[k].look;
        if (!sym || !*sym) {
            int mf = ndlib_map_func_for_pc(chain[k].look);
            sym = mf >= 0 ? ndlib_map_func(mf, &entry, NULL) : NULL;
        }
        if (sym && *sym) {
            f->name = strdup(sym);
            f->valid_symbol = true;
            f->symbol_entry_point = entry;
        } else {
            char buf[32];
            snprintf(buf, sizeof(buf), "0x%08X", pc);
            f->name = strdup(buf);
            f->valid_symbol = false;
        }
        f->instruction_pointer_reference = (int)pc;

        const char* file = NULL;
        int line = 0;
        /* Any source file (C, PLANC, assembly); the line whose address range
         * holds the address. Returns 1 when it finds a line, 0 when not. */
        if (ndlib_symbols_source_for_pc(chain[k].look, &file, &line) == 1 && file && line > 0) {
            f->source_path = strdup(file);
            const char* base = strrchr(file, '/');
            f->source_name = strdup(base ? base + 1 : file);
            f->line = line;
            f->column = 1;
        }
    }

    ctx->frame_count = count;
    ctx->total_frames = count;
    return 0;
}

DAP_CMD_LOCKED(cmd_scopes) {
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
    const int NUM_REG_SCOPES = (int)(sizeof(defs) / sizeof(defs[0]));
    /* the source map's scopes first: Locals of the routine of the frame asked for (its
     * frame base from map_frames), Globals of the module */
    int locals = 0;
    int mf = -1;
    int locals_ref = 0;
    if (g_machine && g_machine->cpu) {
        MapFrame chain[MAP_FRAMES_MAX];
        int count = map_frames(chain, MAP_FRAMES_MAX);
        int k = ctx->frame_id >= 0 && ctx->frame_id < count ? ctx->frame_id : 0;
        mf = ndlib_map_func_for_pc(chain[k].look);
        if (mf >= 0) (void)ndlib_map_func(mf, NULL, &locals);
        if (locals > 0) locals_ref = map_handle(NULL, chain[k].b, 0, mf);
    }
    if (locals_ref == 0) locals = 0;
    int globals = ndlib_map_global_count();
    int n = 0;
    DAPScope* scopes = (DAPScope*)calloc((size_t)NUM_REG_SCOPES + 2, sizeof(DAPScope));
    if (!scopes) return -1;
    if (locals > 0) {
        scopes[n].name = strdup("Locals");
        scopes[n].variables_reference = locals_ref;
        scopes[n].named_variables = locals;
        n++;
    }
    if (globals > 0) {
        scopes[n].name = strdup("Globals");
        scopes[n].variables_reference = SCOPE_ID_GLOBALS;
        scopes[n].named_variables = globals;
        n++;
    }
    for (int i = 0; i < NUM_REG_SCOPES; i++) {
        scopes[n].name = strdup(defs[i].name);
        scopes[n].variables_reference = defs[i].ref;
        scopes[n].named_variables = defs[i].count;
        scopes[n].expensive = false;
        n++;
    }
    ctx->scopes = scopes;
    ctx->scope_count = n;
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

DAP_CMD_LOCKED(cmd_variables) {
    VariablesCommandContext* ctx = &server->current_command.context.variables;
    ctx->variable_array = NULL;
    ctx->variable_count = 0;
    if (!g_machine || !g_machine->cpu) return -1;

    Nd500Cpu* cpu = g_machine->cpu;
    char value[64];

    int vref = ctx->variables_reference;
    if (vref == SCOPE_ID_GLOBALS) {
        for (int i = 0; i < ndlib_map_global_count(); i++) {
            uint32_t addr = 0;
            const char* gn = ndlib_map_global(i, &addr);
            map_add_var(server, gn, addr);
        }
        return 0;
    }
    if (vref >= SCOPE_ID_HANDLE && vref < SCOPE_ID_HANDLE + g_map_handle_count) {
        const MapHandle* h = &g_map_handles[vref - SCOPE_ID_HANDLE];
        if (h->func >= 0) {
            /* the Locals of a frame: each named local at its B + offset */
            int locals = 0;
            (void)ndlib_map_func(h->func, NULL, &locals);
            for (int i = 0; i < locals; i++) {
                int32_t off = 0;
                const char* ln = ndlib_map_local(h->func, i, &off);
                if (ln) map_add_var(server, ln, h->addr + (uint32_t)off);
            }
        } else {
            map_add_children(server, h);
        }
        return 0;
    }

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

DAP_CMD_LOCKED(cmd_set_variable) {
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

DAP_CMD_LOCKED(cmd_evaluate) {
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

/* -- Memory ------------------------------------------------------ */

DAP_CMD_LOCKED(cmd_read_memory) {
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

DAP_CMD_LOCKED(cmd_write_memory) {
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

/* -- Disassembly and symbols ------------------------------------- */

DAP_CMD_LOCKED(cmd_disassemble) {
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

DAP_CMD_LOCKED(cmd_symbol_list) {
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

/* -- Console I/O (custom DAP commands, nd100x convention) -------- */

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

/* -- Server setup / thread --------------------------------------- */

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
    dap_server_set_capability(DAP_CAP_CONDITIONAL_BREAKPOINTS, true);
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
/* At the exit of nd500x: give the DAP thread up to a second to send the end of the program
 * and the logpoint texts still queued (cmd_check_cpu_events sends them; a program that ends right after a logpoint would
 * otherwise take its text along with the process). */
static void dap_flush_log_at_exit(void) {
    for (int i = 0; i < 100 && g_dap_thread_running && g_server; i++) {
        pthread_mutex_lock(&g_log_lock);
        size_t n = g_log_len;
        pthread_mutex_unlock(&g_log_lock);
        if (n == 0 && !g_exit_pending) return;   /* and the end of the program sent */
        struct timespec ts = {0, 10000000}; /* 10 ms */
        nanosleep(&ts, NULL);
    }
}

int nd500_dap_bind(Nd500Machine* m, DAPServer* server) {
    if (m && m->bp_mgr) m->bp_mgr->log_hook = dap_log_hook;   /* logpoints (dap_log_hook) */
    {
        static int at_exit_set = 0;
        if (!at_exit_set) {
            at_exit_set = 1;
            atexit(dap_flush_log_at_exit);
        }
    }
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

int nd500_dap_wait_configured(void) {
    /* nd500x --dap-wait: until a client has sent configurationDone (its breakpoints are set) */
    while (g_dap_thread_running && !(g_server && g_server->debugger_state.configuration_done)) {
        struct timespec ts = {0, 20000000}; /* 20 ms */
        nanosleep(&ts, NULL);
    }
    return (g_server && g_server->debugger_state.configuration_done) ? 0 : -1;
}

int nd500_dap_is_active(void) {
    return g_dap_thread_running;
}

#endif /* DAP_ENABLED */
