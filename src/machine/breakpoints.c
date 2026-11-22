/*
 * breakpoints.c - breakpoint and watchpoint tables
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 Ronny Hansen
 *
 * See LICENSE in the repository root for the full text.
 */

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <ctype.h>
#include "breakpoints.h"
#include "../cpu/cpu_protos.h"
#include "machine_protos.h"

void bp_mgr_init(BreakpointManager* mgr) {
    if (!mgr) return;
    memset(mgr, 0, sizeof(*mgr));
}

/* Breakpoint management */
/* The messages of a breakpoint or watchpoint being set, hit, deleted, enabled or disabled
 * go to stderr, as the other diagnostics of the emulator ([STOPDBG], [PTDBG] in cpu.c):
 * stdout is the console of the ND-500 program, and a debug client that sets breakpoints
 * must not change what the program is seen to print. The tables of bp_list and wp_list
 * are the answer of a REPL command and stay on stdout. */
int bp_add(BreakpointManager* mgr, uint32_t address, bool one_shot) {
    if (!mgr || mgr->bp_count >= MAX_BREAKPOINTS) return -1;

    int id = mgr->bp_count;
    Breakpoint* bp = &mgr->breakpoints[id];
    bp->address = address;
    bp->type = BP_TYPE_EXECUTE;
    bp->enabled = true;
    bp->one_shot = one_shot;
    bp->hit_count = 0;
    bp->log_message[0] = '\0';
    bp->condition[0] = '\0';

    mgr->bp_count++;
    fprintf(stderr, "Breakpoint %d set at 0x%08X%s\n", id, address, one_shot ? " (one-shot)" : "");
    return id;
}

int bp_add_conditional(BreakpointManager* mgr, struct Nd500Cpu* cpu, uint32_t address,
                       const char* condition, bool one_shot) {
    if (!mgr || mgr->bp_count >= MAX_BREAKPOINTS || !condition) return -1;
    if (strlen(condition) >= sizeof(mgr->breakpoints[0].condition)) return -2;

    /* Refuse a condition that does not evaluate now: it never would */
    ExprResult check = bp_evaluate_condition(condition, cpu);
    if (!check.valid) {
        fprintf(stderr, "Conditional breakpoint not set: %s\n", check.error_msg);
        return -2;
    }

    int id = mgr->bp_count;
    Breakpoint* bp = &mgr->breakpoints[id];
    bp->address = address;
    bp->type = BP_TYPE_CONDITIONAL;
    bp->enabled = true;
    bp->one_shot = one_shot;
    bp->hit_count = 0;
    bp->log_message[0] = '\0';
    strncpy(bp->condition, condition, sizeof(bp->condition) - 1);
    bp->condition[sizeof(bp->condition) - 1] = '\0';

    mgr->bp_count++;
    fprintf(stderr, "Conditional breakpoint %d set at 0x%08X: %s%s\n", id, address, condition, one_shot ? " (one-shot)" : "");
    return id;
}

int bp_set_log_message(BreakpointManager* mgr, int id, const char* text) {
    if (!mgr || id < 0 || id >= mgr->bp_count || !text) return -1;
    if (strlen(text) >= sizeof(mgr->breakpoints[id].log_message)) return -1;
    strcpy(mgr->breakpoints[id].log_message, text);
    return 0;
}

int bp_delete(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->bp_count) return -1;

    /* Shift remaining breakpoints down */
    for (int i = id; i < mgr->bp_count - 1; i++) {
        mgr->breakpoints[i] = mgr->breakpoints[i + 1];
    }
    mgr->bp_count--;
    fprintf(stderr, "Breakpoint %d deleted\n", id);
    return 0;
}

int bp_enable(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->bp_count) return -1;
    mgr->breakpoints[id].enabled = true;
    fprintf(stderr, "Breakpoint %d enabled\n", id);
    return 0;
}

int bp_disable(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->bp_count) return -1;
    mgr->breakpoints[id].enabled = false;
    fprintf(stderr, "Breakpoint %d disabled\n", id);
    return 0;
}

void bp_list(BreakpointManager* mgr) {
    if (!mgr || mgr->bp_count == 0) {
        fprintf(stderr, "No breakpoints set\n");
        return;
    }

    fprintf(stderr, "=== BREAKPOINTS ===\n");
    fprintf(stderr, "ID  Address    Enabled  Hits   Type        Condition\n");
    fprintf(stderr, "--- ---------- -------- ------ ----------- ---------\n");
    for (int i = 0; i < mgr->bp_count; i++) {
        Breakpoint* bp = &mgr->breakpoints[i];
        const char* type_str = (bp->type == BP_TYPE_CONDITIONAL) ? "conditional" : "normal";
        fprintf(stderr, "%3d 0x%08X %-8s %6u %-11s %s\n",
               i, bp->address,
               bp->enabled ? "yes" : "no",
               bp->hit_count,
               type_str,
               bp->type == BP_TYPE_CONDITIONAL ? bp->condition : "");
    }
}

bool bp_should_break_at(BreakpointManager* mgr, uint32_t pc, struct Nd500Cpu* cpu) {
    if (!mgr) return false;

    /* Check if we should break on next instruction (step command) */
    if (mgr->break_on_next_instruction) {
        mgr->break_on_next_instruction = false;
        return true;
    }

    /* Check all breakpoints */
    for (int i = 0; i < mgr->bp_count; i++) {
        Breakpoint* bp = &mgr->breakpoints[i];
        if (bp->enabled && bp->address == pc) {
            /* For conditional breakpoints, evaluate the condition */
            if (bp->type == BP_TYPE_CONDITIONAL) {
                ExprResult r = bp_evaluate_condition(bp->condition, cpu);
                if (!r.valid) {
                    /* Cannot decide: stop and say why, never skip silently */
                    fprintf(stderr, "\nBreakpoint %d at 0x%08X: condition '%s' cannot be evaluated: %s\n",
                           i, pc, bp->condition, r.error_msg);
                } else if (r.value == 0) {
                    continue;   /* condition false: no stop from this breakpoint */
                }
            }

            bp->hit_count++;
            if (bp->log_message[0] != '\0') {
                /* a logpoint: say the text, go on (DAP setBreakpoints logMessage) */
                if (mgr->log_hook) mgr->log_hook(bp->log_message, cpu);
                else fprintf(stderr, "%s\n", bp->log_message);
                continue;
            }
            fprintf(stderr, "\nBreakpoint %d hit at 0x%08X (hit count: %u)\n", i, pc, bp->hit_count);

            /* Handle one-shot breakpoints */
            if (bp->one_shot) {
                fprintf(stderr, "One-shot breakpoint %d deleted\n", i);
                bp_delete(mgr, i);
            }
            return true;
        }
    }
    return false;
}

/* Watchpoint management */
int wp_add(BreakpointManager* mgr, uint32_t address, uint32_t length, WatchpointType type) {
    if (!mgr || mgr->wp_count >= MAX_WATCHPOINTS) return -1;

    int id = mgr->wp_count;
    Watchpoint* wp = &mgr->watchpoints[id];
    wp->address = address;
    wp->length = length;
    wp->type = type;
    wp->enabled = true;
    wp->last_value = 0;
    wp->hit_count = 0;
    wp->register_name[0] = '\0';

    mgr->wp_count++;
    const char* type_str = (type == WP_TYPE_READ) ? "read" :
                           (type == WP_TYPE_WRITE) ? "write" :
                           (type == WP_TYPE_CHANGE) ? "change" : "register";
    fprintf(stderr, "Watchpoint %d set at 0x%08X (length=%u, type=%s)\n", id, address, length, type_str);
    return id;
}

int wp_add_register(BreakpointManager* mgr, const char* reg_name, uint32_t reg_index) {
    if (!mgr || mgr->wp_count >= MAX_WATCHPOINTS || !reg_name) return -1;

    int id = mgr->wp_count;
    Watchpoint* wp = &mgr->watchpoints[id];
    wp->address = reg_index;  /* Store register index in address field */
    wp->length = 4;           /* Registers are 32-bit */
    wp->type = WP_TYPE_REGISTER;
    wp->enabled = true;
    wp->last_value = 0;
    wp->hit_count = 0;
    strncpy(wp->register_name, reg_name, sizeof(wp->register_name) - 1);
    wp->register_name[sizeof(wp->register_name) - 1] = '\0';

    mgr->wp_count++;
    fprintf(stderr, "Register watchpoint %d set on %s (index=%u)\n", id, reg_name, reg_index);
    return id;
}

int wp_register_index_for_name(const char* name) {
    static const char* names[WP_REG_INDEX_COUNT] = {
        "PC", "I1", "I2", "I3", "I4", "L", "B", "R"
    };
    if (!name) return -1;
    for (int i = 0; i < WP_REG_INDEX_COUNT; i++) {
        if (strcasecmp(names[i], name) == 0) return i;
    }
    return -1;
}

int wp_check_registers(BreakpointManager* mgr, const uint32_t regs[WP_REG_INDEX_COUNT]) {
    if (!mgr) return -1;
    for (int i = 0; i < mgr->wp_count; i++) {
        Watchpoint* wp = &mgr->watchpoints[i];
        if (!wp->enabled || wp->type != WP_TYPE_REGISTER) continue;
        if (wp->address >= WP_REG_INDEX_COUNT) continue;
        uint32_t value = regs[wp->address];
        if (value != wp->last_value) {
            wp->hit_count++;
            fprintf(stderr, "\nRegister watchpoint %d hit on %s (old=0x%08X new=0x%08X)\n",
                   i, wp->register_name, wp->last_value, value);
            wp->last_value = value;
            return i;
        }
    }
    return -1;
}

int wp_delete(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->wp_count) return -1;

    /* Shift remaining watchpoints down */
    for (int i = id; i < mgr->wp_count - 1; i++) {
        mgr->watchpoints[i] = mgr->watchpoints[i + 1];
    }
    mgr->wp_count--;
    fprintf(stderr, "Watchpoint %d deleted\n", id);
    return 0;
}

int wp_enable(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->wp_count) return -1;
    mgr->watchpoints[id].enabled = true;
    fprintf(stderr, "Watchpoint %d enabled\n", id);
    return 0;
}

int wp_disable(BreakpointManager* mgr, int id) {
    if (!mgr || id < 0 || id >= mgr->wp_count) return -1;
    mgr->watchpoints[id].enabled = false;
    fprintf(stderr, "Watchpoint %d disabled\n", id);
    return 0;
}

void wp_list(BreakpointManager* mgr) {
    if (!mgr || mgr->wp_count == 0) {
        fprintf(stderr, "No watchpoints set\n");
        return;
    }

    fprintf(stderr, "=== WATCHPOINTS ===\n");
    fprintf(stderr, "ID  Address    Length Enabled  Hits   Type\n");
    fprintf(stderr, "--- ---------- ------ -------- ------ ------\n");
    for (int i = 0; i < mgr->wp_count; i++) {
        Watchpoint* wp = &mgr->watchpoints[i];
        const char* type_str = (wp->type == WP_TYPE_READ) ? "read" :
                               (wp->type == WP_TYPE_WRITE) ? "write" : "change";
        fprintf(stderr, "%3d 0x%08X %6u %-8s %6u %s\n",
               i, wp->address, wp->length,
               wp->enabled ? "yes" : "no",
               wp->hit_count, type_str);
    }
}

bool wp_should_break_on_read(BreakpointManager* mgr, uint32_t addr) {
    if (!mgr) return false;

    for (int i = 0; i < mgr->wp_count; i++) {
        Watchpoint* wp = &mgr->watchpoints[i];
        if (wp->enabled &&
            (wp->type == WP_TYPE_READ || wp->type == WP_TYPE_CHANGE) &&
            addr >= wp->address && addr < (wp->address + wp->length)) {
            wp->hit_count++;
            fprintf(stderr, "\nWatchpoint %d hit on read at 0x%08X\n", i, addr);
            return true;
        }
    }
    return false;
}

bool wp_should_break_on_write(BreakpointManager* mgr, uint32_t addr, uint32_t value) {
    if (!mgr) return false;

    for (int i = 0; i < mgr->wp_count; i++) {
        Watchpoint* wp = &mgr->watchpoints[i];
        if (wp->enabled &&
            addr >= wp->address && addr < (wp->address + wp->length)) {

            if (wp->type == WP_TYPE_WRITE) {
                wp->hit_count++;
                fprintf(stderr, "\nWatchpoint %d hit on write at 0x%08X (value=0x%08X)\n", i, addr, value);
                return true;
            }

            if (wp->type == WP_TYPE_CHANGE && value != wp->last_value) {
                wp->hit_count++;
                fprintf(stderr, "\nWatchpoint %d hit on change at 0x%08X (old=0x%08X new=0x%08X)\n",
                       i, addr, wp->last_value, value);
                wp->last_value = value;
                return true;
            }

            /* Update last value for change detection */
            if (wp->type == WP_TYPE_CHANGE) {
                wp->last_value = value;
            }
        }
    }
    return false;
}

/* Condition evaluator for conditional breakpoints.
 * Grammar: REGISTER OP NUMBER (see breakpoints.h). Registers are read
 * through the debugger register table (nd500_dbg_reg_get_by_name), the same
 * names the DAP evaluate request and the CLI 'set' command use. */

static void trim_in_place(char* s) {
    char* start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
        s[--n] = '\0';
    }
}

ExprResult bp_evaluate_condition(const char* condition, struct Nd500Cpu* cpu) {
    ExprResult result = {false, 0, ""};

    if (!condition || condition[0] == '\0') {
        snprintf(result.error_msg, sizeof(result.error_msg), "Empty condition");
        return result;
    }
    if (!cpu) {
        snprintf(result.error_msg, sizeof(result.error_msg), "No CPU to read registers from");
        return result;
    }

    char expr[256];
    snprintf(expr, sizeof(expr), "%s", condition);
    trim_in_place(expr);

    /* The CLI form is written quoted: bp cond <addr> "I1 == 0x1234" */
    size_t len = strlen(expr);
    if (len >= 2 && expr[0] == '"' && expr[len - 1] == '"') {
        expr[len - 1] = '\0';
        memmove(expr, expr + 1, len - 1);
    }

    /* Two-character operators are searched first so ">=" is not read as ">" */
    static const char* const ops[] = { "==", "!=", ">=", "<=", ">", "<" };
    char* op_pos = NULL;
    int op = -1;
    for (int k = 0; k < (int)(sizeof(ops) / sizeof(ops[0])); k++) {
        op_pos = strstr(expr, ops[k]);
        if (op_pos) {
            op = k;
            break;
        }
    }
    if (op < 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), "No comparison (== != >= <= > <)");
        return result;
    }

    size_t op_len = strlen(ops[op]);
    *op_pos = '\0';
    char* name = expr;
    char* value_str = op_pos + op_len;
    trim_in_place(name);
    trim_in_place(value_str);

    uint32_t reg_val = 0;
    if (name[0] == '\0' || nd500_dbg_reg_get_by_name(cpu, name, &reg_val) != 0) {
        snprintf(result.error_msg, sizeof(result.error_msg), "Unknown register '%.40s'", name);
        return result;
    }

    char* endp = NULL;
    unsigned long v = strtoul(value_str, &endp, 0);
    if (value_str[0] == '\0' || !endp || *endp != '\0') {
        snprintf(result.error_msg, sizeof(result.error_msg), "Not a number: '%.40s'", value_str);
        return result;
    }
    uint32_t expected = (uint32_t)v;

    bool holds = false;
    switch (op) {
    case 0: holds = (reg_val == expected); break;
    case 1: holds = (reg_val != expected); break;
    case 2: holds = (reg_val >= expected); break;
    case 3: holds = (reg_val <= expected); break;
    case 4: holds = (reg_val > expected); break;
    default: holds = (reg_val < expected); break;
    }
    result.valid = true;
    result.value = holds ? 1u : 0u;
    return result;
}

bool wp_should_break_on_register_change(BreakpointManager* mgr, uint32_t reg_index, uint32_t value) {
    if (!mgr) return false;

    for (int i = 0; i < mgr->wp_count; i++) {
        Watchpoint* wp = &mgr->watchpoints[i];
        if (wp->enabled && wp->type == WP_TYPE_REGISTER && wp->address == reg_index) {
            if (value != wp->last_value) {
                wp->hit_count++;
                fprintf(stderr, "\nRegister watchpoint %d hit on %s (old=0x%08X new=0x%08X)\n",
                       i, wp->register_name, wp->last_value, value);
                wp->last_value = value;
                return true;
            }
        }
    }
    return false;
}

